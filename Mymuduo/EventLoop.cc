#include"EventLoop.hpp"
#include "Channel.hpp"
#include "TimeStamp.hpp"
#include"log.hpp"
#include"CurrentThread.hpp"
#include "poller.hpp"
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <sys/eventfd.h>
#include <vector>
__thread EventLoop* t_loopthisThread=nullptr;//防止一个线程创建多个loop
const int timeout=10000;
int creatEventfd()
{
    int fd=eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);
    return fd;
}
EventLoop::EventLoop():
looping_(false)
,quit_(false)
,poller_(Poller::Newepoller(this))
,threadId_(CurrentThread::tid())
,weakupfd_(creatEventfd())
,weakupchannel_(new Channel(this,weakupfd_))
,callingpending_(false)
{
	if(t_loopthisThread)//再次创建loop
    {
        Logmanage(FATAL,"repeat creat eventloop");
    }else
    {
        t_loopthisThread=this;
    }
    weakupchannel_->SetrecvCallback(std::bind(&EventLoop::handleRead,this));
    weakupchannel_->SetEnableIn();
}
EventLoop::~EventLoop()
{
    weakupchannel_->remove();
    t_loopthisThread=nullptr;
	close(weakupfd_);
}
void EventLoop::loop()
{
	looping_=true;
    while(!quit_)
    {
        activechannels_.clear();
        pollReturnTime_=poller_->poll(timeout,&activechannels_);
        for(auto& activechannel:activechannels_)
        {
            //注意channel可能已经被析构了r
            activechannel->FunHadler(activechannel->fd());
        }
        //执行回调队列中的函数
        doPendingFunctors();
    }
    looping_=false;
}
void EventLoop::quit()
{
	quit_=true;
    //说明调用该loop时不在该loop本身线程中需要唤醒该loop
    if(!isInLoopThread())
    {
       weakup();
    }
}
void EventLoop::runInLoop(Functor cb)
{
    //执行该cb的loop线程时就是在本身线程中直接执行
	if(isInLoopThread())
    {
        cb();
    }else
    {
        queueInLoop(cb);
    }
}
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mtx_);
        pendingFunctors_.emplace_back(std::move(cb));
    }
    if(!isInLoopThread()||callingpending_==true)
    {
        weakup();
    }
}
void EventLoop::updateChannel(Channel* channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel* channel)
{
	poller_->removeChannel(channel);
}
void EventLoop::weakup()
{
    uint64_t one=1;
    int n=write(weakupfd_,&one,sizeof(one));
    if(n!=sizeof(one))
    {
        Logmanage(ERR,"weakup fail:%d:%s",errno,strerror(errno));
    }
}
void EventLoop::handleRead()
{
    uint64_t one=1;
    int n=read(weakupfd_,&one,sizeof(one));
}
bool EventLoop::isInLoopThread() const
{
    return threadId_==CurrentThread::tid();
}
void EventLoop::doPendingFunctors()
{
	callingpending_=true;
    std::vector<Functor> func;
    {
        std::unique_lock<std::mutex> lock(mtx_);
        func.swap(pendingFunctors_);
    }
    for(auto& cb:func)
    {
        cb();
    }
    callingpending_=false;
}