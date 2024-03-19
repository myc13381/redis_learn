// 该文件封装了 IO多路复用函数 epoll_create epoll_ctl epoll_wait
#include <sys/epoll.h>
#include <errno.h>
#include "ae.h"

class aeApiState
{
public:
    int epfd;
    epoll_event *events;
    aeApiState() {}
};

// 初始化 eventloop 中的 apiData 成员变量
static int aeApiCreate(aeEventLoop &eventloop)
{
    eventloop.apiData.events = new epoll_event[eventloop.setSize];
    eventloop.apiData.epfd = epoll_create(1024); // 1024 只是一个填充值

    // 在给定 fd 上启用FD_CLOEXEC以避免 fd 泄漏。
    // .... 具体实现和 anet.c 有关

    return 0;
}


// 释放 epoll 有关资源
static inline void aeApiFree(aeEventLoop &eventloop)
{
    close(eventloop.apiData.epfd);
    delete [] eventloop.apiData.events;
}

// 向 eventloop 中添加 IO事件 
// 成功返回0，失败返回 -1
static int adApiAddEvent(aeEventLoop &eventloop, int fd, int mask)
{
    epoll_event ee {0};
    // 如果这个事件还没有被触发过，则使用 EPOLL_CTL_ADD, 否则使用 EPOLL_CTL_MOD 进行修改
    int op = eventloop.events[fd].mask = AE_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD; 
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
static void aeApiDelEvent(aeEventLoop &eventloop, int fd, int delmask)
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
static int aeApiPoll(aeEventLoop &eventloop, int waitTime)
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

static std::string aeApiName(void) {
    return "epoll";
}
