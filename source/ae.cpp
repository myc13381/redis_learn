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

void aeMain(Server &server, aeEventLoop &aeLoop, std::vector<threadsafe_queue<IOThreadNews>> &io_q, threadsafe_queue<std::pair<int, Command>> &exe_q)
{
    
    aeLoop.aeEventLoopStop = false;
    while(!aeLoop.aeEventLoopStop && !server.serverStop)
    {
        std::chrono::milliseconds startTime =  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock().now().time_since_epoch());
        // 处理时间事件
        aeLoop.dealWithTimeEvents(server);
        // 处理循环的时间事件
        serverCron(server);
        
        // 处理IO事件
        size_t eventNum = 0;
        // if(server.fdSet.size() < 20) 
        eventNum = aeApiPoll(aeLoop, 500);
        if(eventNum == -1) eventNum = 0;
        
        // 不开启 IO 多线程
        if(server.IOThreadNum == 0)
        {
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
        else // 开启 IO 多线程
        {
            for(size_t i=0;i<eventNum;++i)
            {
                int fd = aeLoop.fired[i].fd;
                int mask = aeLoop.fired[i].mask;
                std::cout<<fd<<"\n";
                if(fd == server.config.master_socket_fd && (mask & EPOLLIN))
                {
                    // 连接客户端
                    aeServerConnectToClient(server,aeLoop,nullptr);
                }
                else
                { // 读取客户端发来的信息
                    if(server.fdSet.count(fd)==0)
                    {
                        io_q[fd%server.IOThreadNum].push(IOThreadNews(fd, true, std::string()));
                        std::cout<<"get message from client\n";
                        server.fdSet.insert(fd);
                    }
                }
            }

            // 执行数据库修改操作
            std::pair<int, Command> p;
            std::string retMessage;
            while(!exe_q.empty())
            {
                if(exe_q.try_pop(p))
                {
                    retMessage = execCommand(server, p.second);
                }
                io_q[p.first%server.IOThreadNum].push(IOThreadNews(p.first, false, retMessage)); // 将执行的结过发送给客户端
            }
        }


        std::chrono::milliseconds endTime =  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock().now().time_since_epoch());
        int circleTime = (endTime - startTime).count();
        if(circleTime == 0) circleTime = 1000;
        server.hz = 1000/circleTime > 0 ? 1000/circleTime : 1;
        continue;
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
    event.mask |= mask;
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

    // 设置为非阻塞
    int flag = fcntl(clientFd, F_GETFL, 0);
    fcntl(clientFd, F_SETFL, flag|O_NONBLOCK);

    aeCreateFileEvent(clientFd, aeloop, readQueryFromClient, AE_READABLE, nullptr);
}

// 读取客户端的发送的数据并处理
void readQueryFromClient(int fd, Server &server, aeEventLoop &aeLoop, void *clientData)
{
    constexpr size_t BUFFSIZE = 8192;
    char buff[BUFFSIZE];
    // 首先读取客户端发送数据的长度
    size_t len = 0;
    int readLen = 0;
    while(readLen != sizeof(size_t))
    {
        readLen += read(fd, static_cast<void*>(&len+readLen), sizeof(size_t));
        if(readLen == 0) 
        {   // 客户端断开连接
            closeClient(fd, server, aeLoop);
            return;
        }
    }
    // len中存储着接下来要发送的数据的长度
    readLen = 0;
    while(readLen != len)
    {
        readLen += read(fd, buff+readLen, len);
    }
    buff[len] = 0;
    // std::cout<<buff<<std::endl;
    Command cmd;
    parseBinaryCmd(buff,cmd); // 解析cmd
    showCommand(cmd);
    // 执行命令并获取结果
    std::string retMessage = execCommand(server,cmd);

    // 向客户端发送命令的执行情况
    len = retMessage.length() + 1;
    write(fd, static_cast<void*>(&len), sizeof(size_t));
    write(fd, retMessage.c_str(), len);
    return;
}

// 关闭客户端
void closeClient(int fd, Server &server, aeEventLoop &aeLoop)
{
    close(fd); // 关闭客户端
    aeApiDelEvent(aeLoop,fd,AE_WRITABLE);
    aeLoop.events[fd].mask = AE_NONE;
    if(aeLoop.events[fd].clientData != nullptr) delete aeLoop.events[fd].clientData;
    aeLoop.events[fd].clientData = nullptr;
    std::cout<<"close client, fd =="<<fd<<std::endl;
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
    ee.events |= EPOLLET; // 采用边缘触发？
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
    // else if(retval == -1 && errno == EINTR)
    // {
    //     // 出现异常
    //     std::cerr<<"aeApiPoll: epoll_wait"<<strerror(errno)<<std::endl;
    //     abort();
    // }
    return numevent; // 返回事件的数量
}

std::string aeApiName(void) {
    return "epoll";
}

// IO 线程运行函数
void IOThreadMain(Server &server, aeEventLoop &aeloop, threadsafe_queue<IOThreadNews> &io_q, threadsafe_queue<std::pair<int, Command>> &exe_q)
{
    // 从 任务队列中取出任务
    IOThreadNews news;
    constexpr size_t BUFFSIZE = 8192;
    char buff[BUFFSIZE];
    while(true)
    {
        if(!io_q.try_pop(news))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }
        if(news.isRead)
        { // 读任务 根据 readLen的值判断现在读哪个部分
            // 不断尝试读取指令，直到读完
            while(true)
            {
                // 首先读取客户端发送数据的长度
                size_t len = 0;
                int readLen = 0;
                bool exit = false;
                readLen = read(news.fd, static_cast<void*>(&len), sizeof(size_t));
                if(readLen < 0 && errno == EAGAIN) 
                    break; // 非阻塞IO，没有数据可读
                if(readLen == 0)
                { // 客户端关闭
                    closeClient(news.fd, server, aeloop);
                    server.fdSet.erase(news.fd);
                    break;
                }
                // len中存储着接下来要发送的数据的长度
                readLen = read(news.fd, buff, len);
                buff[len] = 0;
                // std::cout<<buff<<std::endl;
                Command cmd;
                parseBinaryCmd(buff,cmd); // 解析cmd
                showCommand(cmd);
                std::cout<<news.fd<<"\n";
                // 将解析的命令放入到执行队列中
                exe_q.push(std::make_pair(news.fd, cmd));
            }

            // 下面是之前的代码，使用的是epoll ET，但是阻塞IO :)
            // // 首先读取客户端发送数据的长度
            // size_t len = 0;
            // int readLen = 0;
            // bool exit = false;
            // while(!exit && readLen != sizeof(size_t))
            // {
            //     readLen += read(news.fd, static_cast<void*>(&len+readLen), sizeof(size_t));
            //     if(readLen == 0) 
            //     {   // 客户端断开连接
            //         closeClient(news.fd, server, aeloop);
            //         exit = true;
            //         server.fdSet.erase(news.fd);
            //     }
            // }
            // if(exit) continue; // 客户端终止连接
            // // len中存储着接下来要发送的数据的长度
            // readLen = 0;
            // while(readLen != len)
            // {
            //     readLen += read(news.fd, buff+readLen, len);
            // }
            // buff[len] = 0;
            // // std::cout<<buff<<std::endl;
            // Command cmd;
            // parseBinaryCmd(buff,cmd); // 解析cmd
            // showCommand(cmd);
            // // 将解析的命令放入到执行队列中
            // exe_q.push(std::make_pair(news.fd, cmd));
        }
        else
        { // 写任务
            // 向客户端发送命令的执行情况
            size_t len = news.str.length() + 1;
            write(news.fd, static_cast<void*>(&len), sizeof(size_t));
            //std::this_thread::sleep_for(std::chrono::milliseconds(500));
            write(news.fd, news.str.c_str(), len);
            //std::this_thread::sleep_for(std::chrono::milliseconds(50));
            server.fdSet.erase(news.fd);
            
        }
    }
}