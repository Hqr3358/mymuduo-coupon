#include "EventLoop.hpp"
#include"LoopThread.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
/** @brief IO 线程：一个线程一个 EventLoop（sub-reactor 线程模型） */
class EventLoopThread
{
    using ThreadInitCallback=std::function<void(EventLoop*)>;
public:
	// 构造：保存线程初始化回调
	explicit EventLoopThread(const ThreadInitCallback& cb=ThreadInitCallback());
    // 析构
    ~EventLoopThread();
	EventLoop* startloop();//执行loop循环并返回自己loop指针
    // 线程函数：创建并运行 EventLoop
    void threadFunc();
private:
	std::unique_ptr<Thread> thread_;//自己封装的thread线程类
    std::atomic_bool exit_;
   	EventLoop* loop_;//自己的loop线程
    ThreadInitCallback callback_;
	std::mutex mtx_;
    std::condition_variable cv_;
};