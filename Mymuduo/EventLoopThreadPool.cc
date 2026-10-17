#include"EventLoopThreadPool.hpp"
#include "log.hpp"
#include <cmath>
#include <memory>

int EventLoopThreadPool::next_=0;
EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseloop):
started_(false)
,baseloop_(baseloop)
{}
EventLoopThreadPool::~EventLoopThreadPool(){}
void EventLoopThreadPool::Setnumthread(int numthread)
{
    numthread_=numthread;
}
void EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
	for(int i=0;i<numthread_;i++)
    {
        threads_.emplace_back(std::make_unique<EventLoopThread>(cb));
        loops_.emplace_back(threads_[i]->startloop());
    }
    Logmanage(NORMOL, "thread num:%d",numthread_);
    if(numthread_==0&&cb)
    {
        cb(baseloop_);
    }
}
EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop* loop=baseloop_;
	if(!loops_.empty())
    {
        loop=loops_[next_++];
        if(next_>=numthread_)
        {
			next_=0;
        }
    }
    return loop;
}