#pragma once
// 实现线程安全的队列
#include <mutex>
#include <memory> // shared_ptr
#include <condition_variable>
#include <queue>
#include <unordered_set>

template <typename T>
class threadsafe_queue
{
public:
    threadsafe_queue() = default;
    threadsafe_queue(const threadsafe_queue &other)
    {
        std::lock_guard<std::mutex> lk(other.mtx);
        data_queue = other.data_queue;
    }
    void push(T value)
    {
        std::lock_guard<std::mutex> lk(mtx);
        data_queue.push(value);
        data_cond.notify_one(); // 唤醒 wait_and_pop
    }
    void wait_and_pop(T &value)
    {
        std::unique_lock<std::mutex> lk(mtx);
        data_cond.wait(lk, [this](){return !data_queue.empty();}); // lambda 表达式传入this
        value = data_queue.front();
        data_queue.pop();
    }
    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lk(mtx);
        data_cond(lk, [this](){return !data_queue.empty();});
        std::shared_ptr<T> ret(std::make_shared<T>(data_queue.front()));
        data_queue.pop();
        return ret;
    }
    bool try_pop(T &value)
    {
        std::lock_guard<std::mutex> lk(mtx);
        if(data_queue.empty()) {  return false; }
        value = data_queue.front();
        data_queue.pop();
        return true;
    }
    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(mtx);
        if(data_queue.empty()) return std::shared_ptr<T>();
        std::shared_ptr ret = std::make_shared<T>(data_queue.front());
        data_queue.pop();
        return ret;
    }
    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mtx);
        return data_queue.empty();
    }
private:
    mutable std::mutex mtx;
    std::queue<T> data_queue;
    std::condition_variable data_cond; // 同步
};

template <typename T>
class threadsafe_unordered_set
{
public:
    threadsafe_unordered_set<T>() = default;
    std::size_t count(const T &value)
    {
        std::lock_guard<std::mutex> lk(mtx);
        return set.count(value);
    }
    void insert(const T &value)
    {
        std::lock_guard<std::mutex> lk(mtx);
        set.insert(value);
    }
    void erase(const T &value)
    {
        std::lock_guard<std::mutex> lk(mtx);
        set.erase(value);
    }
    bool empty()
    {
        std::lock_guard<std::mutex> lk(mtx);
        return set.empty();
    }
    std::size_t size()
    {
        std::lock_guard<std::mutex> lk(mtx);
        return set.size();
    }
private:
    std::mutex mtx;
    std::unordered_set<T> set;
};