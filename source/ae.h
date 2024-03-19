#ifndef REDIS_LEARN_AE
#define REDIS_LEARN_AE
// 事件驱动模型

#include <list>
#include <chrono>
#include <functional>

#include "server.h"

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

// IO 事件
class aeFileEvent
{
public:
    int mask; // 类型掩码 0 为缺省值，1 表示读事件， 2 表示写事件, 4 表示屏障事件
    std::function<void(Server &, void *)> rfileProc;
    std::function<void(Server &, void *)> wfileProc;
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

    void initAeApiState();
private:
};

void aeMain(Server &server, aeEventLoop &aeLoop);

#endif // REDIS_LEARN_AE