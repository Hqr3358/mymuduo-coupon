#include "EventLoop.hpp"
#include"LoopThread.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
class EventLoopThread
{
    using ThreadInitCallback=std::function<void(EventLoop*)>;
public:
	explicit EventLoopThread(const ThreadInitCallback& cb=ThreadInitCallback());
    ~EventLoopThread();
	EventLoop* startloop();//执行loop循环并返回自己loop指针
    void threadFunc();
private:
	std::unique_ptr<Thread> thread_;//自己封装的thread线程类
    std::atomic_bool exit_;
   	EventLoop* loop_;//自己的loop线程
    ThreadInitCallback callback_;
	std::mutex mtx_;
    std::condition_variable cv_;
};