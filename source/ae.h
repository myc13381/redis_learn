#ifndef REDIS_LEARN_AE
#define REDIS_LEARN_AE
// 事件驱动模型

#include <list>
#include <chrono>
#include <functional>

#include "server.h"


// 事件时间
class aeTimeEvent
{
public:
    long long id;
    std::chrono::milliseconds ms;
    std::function<void(Server &)> timeEventProc; // 时间事件处理函数
    aeTimeEvent() {}
private:
};

// 事件池
class aeEventLoop
{
public:
    size_t numberOfTimeEvent;
    std::list<aeTimeEvent> aeTimeEventList;
    bool aeEventLoopStop;
    // 处理时间事件
    void addTimeEventToLoop(std::function<void(Server &)> func, std::chrono::milliseconds ms); // 添加时间事件
    void dealWithTimeEvents(Server &server); // 处理时间事件
    // 处理IO事件
    aeEventLoop():aeEventLoopStop(false) {}
private:
};

void aeMain(Server &server, aeEventLoop &aeLoop);

#endif // REDIS_LEARN_AE