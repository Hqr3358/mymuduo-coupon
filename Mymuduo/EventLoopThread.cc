#include"EventLoopThread.hpp"
#include "EventLoop.hpp"
#include <mutex>
/** @brief 构造：保存线程回调，线程启动后执行 threadFunc */
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb):
thread_(std::make_unique<Thread>(std::bind(&EventLoopThread::threadFunc,this)))
,exit_(false)
,loop_(nullptr)
,callback_(cb){}
/** @brief 析构：默认（线程随进程退出） */
EventLoopThread::~EventLoopThread()
{
    exit_=true;
    if(loop_!=nullptr)
    {
        loop_->quit();
    	thread_->join();
    }
}
/** @brief 启动线程并返回该线程的事件循环（等待就绪信号） */
EventLoop* EventLoopThread::startloop()
{
	thread_->start();
	{
       	std::unique_lock<std::mutex> lock(mtx_);
		while(loop_==nullptr)
        {
            cv_.wait(lock);
        }
    }
    return loop_;
}
/** @brief 线程函数：创建 EventLoop 并运行主循环 */
void EventLoopThread::threadFunc()
{
	EventLoop loop;
    if(callback_)
    {
        callback_(&loop);
    }
    {
        std::unique_lock<std::mutex> lock(mtx_);
        loop_=&loop;
    	cv_.notify_one();
    }
    loop.loop();
    std::unique_lock<std::mutex> lock(mtx_);
	loop_=nullptr;
}