// 线程池
#ifndef TRINITY_THREAD_POOL_H
#define TRINITY_THREAD_POOL_H

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <thread>

namespace Trinity
{
class ThreadPool
{
public:
    // 取当前支持的最大线程数来分配线程池
    explicit ThreadPool(std::size_t numThreads = std::thread::hardware_concurrency()) : _impl(numThreads) { }

    // 提交一个任务到线程池
    template<typename T>
    decltype(auto) PostWork(T&& work)
    {
        return boost::asio::post(_impl, std::forward<T>(work));
    }
    // 等待所有线程结束
    void Join()
    {
        _impl.join();
    }

private:
    boost::asio::thread_pool _impl;
};
}

#endif // TRINITY_THREAD_POOL_H
