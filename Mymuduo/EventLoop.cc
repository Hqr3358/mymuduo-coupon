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
/** @brief 创建 eventfd（非阻塞 + CLOEXEC），用于跨线程唤醒事件循环 */
int creatEventfd()
{
    int fd=eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);
    return fd;
}
/** @brief 构造事件循环：创建 epoll poller + eventfd 唤醒通道，绑定所属 IO 线程 */
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
/** @brief 析构：移除唤醒通道并关闭 eventfd，置空线程局部指针 */
EventLoop::~EventLoop()
{
    weakupchannel_->remove();
    t_loopthisThread=nullptr;
	close(weakupfd_);
}
/** @brief 【功能】事件循环主循环：epoll_wait 阻塞等待 + 分发就绪事件 + 执行 pending 任务 */
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
/** @brief 退出事件循环（置 quit_ 标志，唤醒后自然退出） */
void EventLoop::quit()
{
	quit_=true;
    //说明调用该loop时不在该loop本身线程中需要唤醒该loop
    if(!isInLoopThread())
    {
       weakup();
    }
}
/** @brief 在 IO 线程执行任务：本线程直接执行，跨线程转 queueInLoop 投递 */
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
/** @brief 把任务挂入 pending 队列并唤醒 IO 线程（跨线程安全投递） */
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
/** @brief 注册/更新 Channel 到 poller（加入事件监听） */
void EventLoop::updateChannel(Channel* channel)
{
    poller_->updateChannel(channel);
}
/** @brief 从 poller 移除 Channel（取消事件监听） */
void EventLoop::removeChannel(Channel* channel)
{
	poller_->removeChannel(channel);
}
/** @brief 唤醒 IO 线程（eventfd 写入，使阻塞中的 poll 立即返回） */
void EventLoop::weakup()
{
    uint64_t one=1;
    int n=write(weakupfd_,&one,sizeof(one));
    if(n!=sizeof(one))
    {
        Logmanage(ERR,"weakup fail:%d:%s",errno,strerror(errno));
    }
}
/** @brief 唤醒事件处理：读取并清空 eventfd 计数 */
void EventLoop::handleRead()
{
    uint64_t one=1;
    int n=read(weakupfd_,&one,sizeof(one));
}
/** @brief 判断当前线程是否为该事件循环所属 IO 线程 */
bool EventLoop::isInLoopThread() const
{
    return threadId_==CurrentThread::tid();
}
/** @brief 执行 pending 任务队列（跨线程投递的回调在此运行） */
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