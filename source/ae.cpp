#include "ae.h"

// ====================================时间事件====================================
// 添加时间事件
void aeEventLoop::addTimeEventToLoop(std::function<void(Server &)> func, std::chrono::milliseconds ms)
{
    aeTimeEvent event;
    event.ms = ms;
    event.timeEventProc = func;
    this->aeTimeEventList.push_back(event);
}

// 处理时间事件
void aeEventLoop::dealWithTimeEvents(Server &server)
{
    for(auto it = this->aeTimeEventList.begin();it != this->aeTimeEventList.end();++it)
    {
        if(it->ms > std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())) 
            continue;
        it->timeEventProc(server);
        if(it->ms.count() >= 0) this->aeTimeEventList.erase(it);
    }
}

void aeMain(Server &server, aeEventLoop &aeLoop)
{
    aeLoop.aeEventLoopStop = false;
    while(!aeLoop.aeEventLoopStop)
    {
        // 处理时间事件
        //aeLoop.dealWithTimeEvents(server);

        // 处理IO事件
        size_t eventNum = aeApiPoll(aeLoop, -1);

        for(size_t i=0;i<eventNum;++i)
        {
            int fd = aeLoop.fired[i].fd;
            std::cout<<fd<<"\n";
            if(fd == server.config.master_socket_fd)
            {
                // 连接客户端
                aeServerConnectToClient(server,aeLoop,nullptr);
            }
            else readQueryFromClient(fd, server,aeLoop,nullptr);
        }
    }
}

// ==============================IO 事件处理===============================
// 添加 IO 事件
void aeCreateFileEvent(int fd, aeEventLoop &eventloop, std::function<void(int, Server &, aeEventLoop &, void*)> func, int mask, void *clientData)
{
    
    if(aeApiAddEvent(eventloop, fd, mask) == -1)
    {
        errorHandling("aeApiAddEvent error!");
    }
    aeFileEvent &event = eventloop.events[fd]; // 添加事件
    //event.mask |= mask;
    event.mask = mask;
    if(mask & AE_READABLE) event.rfileProc = func;
    if(mask & AE_WRITABLE) event.wfileProc = func;
    event.clientData = clientData;
    if(fd > eventloop.maxfd) eventloop.maxfd = fd;
}




// 服务器连接客户端
void aeServerConnectToClient(Server &server, aeEventLoop &aeloop, void*)
{
    int listenFd = server.config.master_socket_fd;
    sockaddr_in client_addr;
    socklen_t slave_addr_size = sizeof(client_addr);
    int clientFd = accept(listenFd, reinterpret_cast<sockaddr *>(&client_addr), &slave_addr_size);
    int optval = 1;
    setsockopt(clientFd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    aeCreateFileEvent(clientFd, aeloop, readQueryFromClient, AE_WRITABLE, nullptr);
}

// 读取客户端的发送的数据并处理
void readQueryFromClient(int fd, Server &server, aeEventLoop &aeLoop, void *clientData)
{
    char buff[11];
    buff[11]=0;
    int readlen = read(fd,buff,11);
    if(readlen == 0) 
    {
        closeClient(fd, server, aeLoop);
        return;
    }
    std::cout<<buff<<std::endl;
}

// 关闭客户端
void closeClient(int fd, Server &server, aeEventLoop &aeLoop)
{
    close(fd); // 关闭客户端
    aeApiDelEvent(aeLoop,fd,AE_WRITABLE);
    aeLoop.events[fd].mask = AE_NONE;
    if(aeLoop.events[fd].clientData != nullptr) delete aeLoop.events[fd].clientData;
    aeLoop.events[fd].clientData = nullptr;
    return;
}


// ==========================封装 epoll ==========================================
// 初始化 eventloop 中的 apiData 成员变量
int aeApiCreate(aeEventLoop &eventloop)
{
    eventloop.apiData.events = new epoll_event[eventloop.setSize];
    eventloop.apiData.epfd = epoll_create(1024); // 1024 只是一个填充值

    // 在给定 fd 上启用FD_CLOEXEC以避免 fd 泄漏。
    return 0;
}


// 释放 epoll 有关资源
void aeApiFree(aeEventLoop &eventloop)
{
    close(eventloop.apiData.epfd);
    delete [] eventloop.apiData.events;
}

// 向 eventloop 中添加 IO事件 
// 成功返回0，失败返回 -1
int aeApiAddEvent(aeEventLoop &eventloop, int fd, int mask)
{
    epoll_event ee {0};
    // 如果这个事件还没有被触发过，则使用 EPOLL_CTL_ADD, 否则使用 EPOLL_CTL_MOD 进行修改
    int op = eventloop.events[fd].mask == AE_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD; 
    ee.events = 0;
    mask |= eventloop.events[fd].mask; // 和以前的事件掩码合并
    // 根据 mask 的值判断这个事件的性质
    if(mask & AE_READABLE) ee.events |= EPOLLIN;
    if(mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;
    // 使用 epoll_ctl 添加需要监听的事件
    if(epoll_ctl(eventloop.apiData.epfd, op, fd, &ee)) return -1;
    return 0;
}

// 删除需要监听的事件
// 一个事件可能同时是读写事件
// 使用 delmask 确定删除的是哪一种类型
void aeApiDelEvent(aeEventLoop &eventloop, int fd, int delmask)
{
    epoll_event ee {0};
    int mask = eventloop.events[fd].mask & (~delmask); // 删去要监听的事件类型

    ee.events = 0;
    if(mask & AE_READABLE) ee.events |= EPOLLIN;
    if(mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;
    if(mask != AE_NONE) // 如果 mask != AE_NONE 代表还有某种类型的事件需要监听
    {
        epoll_ctl(eventloop.apiData.epfd, EPOLL_CTL_MOD, fd, &ee);
    }
    else // 没有事件监听，直接删除事件
    {
        epoll_ctl(eventloop.apiData.epfd, EPOLL_CTL_DEL, fd, &ee);
    }
}

// 调用 epoll_wait 进行事件的监听
// waitTime 参数表示等待的事件，单位是毫秒, -1 代表永久等待
int aeApiPoll(aeEventLoop &eventloop, int waitTime)
{
    int retval = 0, numevent = 0;
    retval = epoll_wait(eventloop.apiData.epfd, eventloop.apiData.events, eventloop.setSize, waitTime);
    if(retval > 0)
    {
        numevent = retval;
        for(int i=0;i<numevent;++i)
        {
            epoll_event *e = eventloop.apiData.events + i;
            
            int mask = 0;
            if(e->events & EPOLLIN)  mask |= AE_READABLE;
            if(e->events & EPOLLOUT) mask |= AE_WRITABLE;
            if(e->events & EPOLLERR) mask |= AE_READABLE|AE_WRITABLE; // 对应文件描述符发生错误
            if(e->events & EPOLLHUP) mask |= AE_READABLE|AE_WRITABLE; // 对应文件描述符被挂断

            eventloop.fired[i].fd = e->data.fd;
            eventloop.fired[i].mask = mask;
        }
    }
    else if(retval == -1 && errno == EINTR)
    {
        // 出现异常
        std::cerr<<"aeApiPoll: epoll_wait"<<strerror(errno)<<std::endl;
        abort();
    }
    return numevent; // 返回事件的数量
}

std::string aeApiName(void) {
    return "epoll";
}