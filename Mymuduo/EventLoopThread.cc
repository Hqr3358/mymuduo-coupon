#include"EventLoopThread.hpp"
#include "EventLoop.hpp"
#include <mutex>
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb):
thread_(std::make_unique<Thread>(std::bind(&EventLoopThread::threadFunc,this)))
,loop_(nullptr)
,exit_(false)
,callback_(cb){}
EventLoopThread::~EventLoopThread()
{
    exit_=true;
    if(loop_!=nullptr)
    {
        loop_->quit();
    	thread_->join();
    }
}
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