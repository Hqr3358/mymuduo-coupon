#include"Channel.hpp"
#include <linux/close_range.h>
#include<iostream>
#include <memory>
#include <sys/epoll.h>
#include"EventLoop.hpp"
#include "TcpConnnction.hpp"
#include "TimeStamp.hpp"
/** @brief 更新本 Channel 到事件循环（注册/修改监听事件） */
void Channel::update()
{
    if(events_==0)
    {
        loop_->removeChannel(this);
    }else loop_->updateChannel(this);
}
/** @brief 从事件循环移除本 Channel */
void Channel::remove()
{
    loop_->removeChannel(this);
}
/** @brief 事件分发：按 revents 调用读/写/关闭/错误回调 */
void Channel::FunHadler(int fd)
{
    if(tied_)
    {
        auto conn=tie_.lock();
        if(conn)
        {
           HandleEvent(fd); 
        }
    }
    else  HandleEvent(fd);
}
/** @brief 事件总入口：持弱引用锁保证对象存活后分发 */
void Channel::HandleEvent(int fd)
{
    if(revents_&EPOLLHUP)
    {
        if(CloseCallback_)
        {
			std::cout<<"EPOLLHUP ready fd:"<<fd<<std::endl;
            CloseCallback_();
        }
    }
    if(revents_&EPOLLERR)
    {
        if(ErrCallback_)
        {
            ErrCallback_();
        }
    }
    if(revents_&EPOLLIN)
    {
        if(recvCallback_)
        {
			std::cout<<"read ready fd:"<<fd<<std::endl;
            TimeStamp timeout;
            recvCallback_(timeout);
        }
    }
    if(revents_&EPOLLOUT)
    {
        if(WriteCallback_)
        {
			std::cout<<"write ready fd:"<<fd<<std::endl;
            WriteCallback_();
        }
    }
}
/** @brief 绑定对象弱引用（延长生命周期，防止回调时对象已析构） */
void Channel::tie(const std::shared_ptr<void>& cb)
{
    tied_=true;
    tie_=cb;
}

