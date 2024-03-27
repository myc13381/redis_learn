#ifndef REDIS_LEARN_AE
#define REDIS_LEARN_AE
// 事件驱动模型

#include <list>
#include <chrono>
#include <functional>
#include <sys/epoll.h>
#include <errno.h>
#include <fcntl.h> // 设置非阻塞 IO
#include "server.h"
#include "threadsafe_structures.h"

#define AE_NONE 0       /* No events registered. */
#define AE_READABLE 1   /* Fire when descriptor is readable. */
#define AE_WRITABLE 2   /* Fire when descriptor is writable. */
#define AE_BARRIER 4    
/*  With WRITABLE, never fire the event if the
    READABLE event already fired in the same event
    loop iteration. Useful when you want to persist
    things to disk before sending replies, and want
    to do that in a group fashion. 
*/

// forward declare
class aeFileEvent;
class aeFiredEvent;
class aeTimeEvent;
class aeEventLoop;
class aeApiState;


class aeApiState
{
public:
    int epfd;
    epoll_event *events;
    aeApiState() {}
};

// 封装 epoll
int aeApiCreate(aeEventLoop &eventloop);
void aeApiFree(aeEventLoop &eventloop);
int aeApiAddEvent(aeEventLoop &eventloop, int fd, int mask);
void aeApiDelEvent(aeEventLoop &eventloop, int fd, int delmask);
int aeApiPoll(aeEventLoop &eventloop, int waitTime);
std::string aeApiName(void);

// IO 事件
class aeFileEvent
{
public:
    int mask; // 类型掩码 0 为缺省值，1 表示读事件， 2 表示写事件, 4 表示屏障事件
    std::function<void(int, Server &, aeEventLoop &, void *)> rfileProc;
    std::function<void(int, Server &, aeEventLoop &, void *)> wfileProc;
    void *clientData;  // 客户端发来的数据
    aeFileEvent() : mask(AE_NONE), clientData(nullptr) {}
private:
};

// 时间事件
class aeTimeEvent
{
public:
    long long id;
    std::chrono::milliseconds ms;
    std::function<void(Server &)> timeEventProc; // 时间事件处理函数
    aeTimeEvent() : ms(std::chrono::milliseconds(0)) {}
private:
};

// 已触发的事件
class aeFiredEvent
{
public:
    int fd; // 文件描述符
    int mask; // 类型掩码
};

// 事件池
class aeEventLoop
{
public:
    int maxfd; /* highest file descriptor currently registered */
    int setSize; /* max number of file descriptors tracked */

    // IO event
    aeFileEvent *events; // 已经注册的 IO 事件
    aeFiredEvent *fired; // 触发的 IO 事件
    aeApiState apiData;
    // timeEvent
    size_t numberOfTimeEvent;
    std::list<aeTimeEvent> aeTimeEventList;
    bool aeEventLoopStop; // 事件循环停止标志
    // 处理时间事件
    void addTimeEventToLoop(std::function<void(Server &)> func, std::chrono::milliseconds ms); // 添加时间事件
    void dealWithTimeEvents(Server &server); // 处理时间事件

    // before .... after
    std::function<void(Server &)> beforeSleep;
    std::function<void(Server &)> afterSleep;

    // 处理标志
    int flags;

    aeEventLoop() : aeEventLoopStop(false), maxfd(INT8_MAX), setSize(INT8_MAX), flags(0), events(nullptr), fired(nullptr)
    {
        events = new aeFileEvent[this->setSize];
        fired = new aeFiredEvent[this->setSize];
        aeApiCreate(*this);
    }
    aeEventLoop(int setSize) : aeEventLoopStop(false), maxfd(INT8_MAX), setSize(setSize), flags(0), events(nullptr), fired(nullptr)
    {
        events = new aeFileEvent[this->setSize];
        fired = new aeFiredEvent[this->setSize];
        aeApiCreate(*this);
    }

    ~aeEventLoop()
    {
        aeApiFree(*this);
        if(events != nullptr) delete [] events;
        if(fired != nullptr) delete [] fired;
    }

    aeEventLoop& operator=(const aeEventLoop &el) = delete;
private:
};

// IO 线程处理的信息
class IOThreadNews
{
public:
    int fd;
    bool isRead;
    std::string str;
    IOThreadNews() = default;
    IOThreadNews(int fd, bool isRead, std::string str) : fd(fd), isRead(isRead), str(str) {}
    IOThreadNews& operator=(const IOThreadNews &news)
    {
        this->fd = news.fd;
        this->isRead = news.isRead;
        this->str = news.str;
        return *this;
    }
};

// 添加 IO 事件
void aeCreateFileEvent(int fd, aeEventLoop &eventloop, std::function<void(Server &, aeEventLoop &, void*)> func, int mask, void *clientData);

// 事件循环函数
void aeMain(Server &server, aeEventLoop &aeLoop, std::vector<threadsafe_queue<IOThreadNews>> &io_q, threadsafe_queue<std::pair<int, Command>> &exe_q);


// 服务器连接客户端
void aeServerConnectToClient(Server &server, aeEventLoop &aeloop, void*);

// 读取客户端的发送的数据并处理
// 客户端和服务器的沟通方式
// 双发在发送内容前首先发送一个size_t大小的数据包，表明接下来要发送的数据包的长度
// 发送完长度之后才会正在发送数据包
void readQueryFromClient(int fd, Server &server, aeEventLoop &aeLoop, void *clientData);

// 关闭客户端
void closeClient(int fd, Server &server, aeEventLoop &aeLoop);



// IO 线程运行函数
void IOThreadMain(Server &server, aeEventLoop &aeloop, threadsafe_queue<IOThreadNews> &io_q, threadsafe_queue<std::pair<int, Command>> &exe_q);


#endif // REDIS_LEARN_AE