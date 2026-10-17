#include"EventLoopThreadPool.hpp"
#include "log.hpp"
#include <cmath>
#include <memory>

int EventLoopThreadPool::next_=0;
/** @brief 构造 IO 线程池：保存主循环指针，线程数默认 0（单线程模式） */
EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseloop):
started_(false)
,baseloop_(baseloop)
{}
/** @brief 析构：默认（线程随进程退出） */
EventLoopThreadPool::~EventLoopThreadPool(){}
/** @brief 设置 IO 线程数 */
void EventLoopThreadPool::Setnumthread(int numthread)
{
    numthread_=numthread;
}
/** @brief 启动线程池：按线程数创建 EventLoopThread */
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
/** @brief 轮询返回下一个 EventLoop（负载均衡：新连接分派） */
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