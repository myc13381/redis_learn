#include "ae.h"

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
        if(it->ms > std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())) continue;
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
        aeLoop.dealWithTimeEvents(server);

        // 处理IO事件
    }
}