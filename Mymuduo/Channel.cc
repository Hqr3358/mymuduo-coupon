#include"Channel.hpp"
#include <linux/close_range.h>
#include<iostream>
#include <memory>
#include <sys/epoll.h>
#include"EventLoop.hpp"
#include "TcpConnnction.hpp"
#include "TimeStamp.hpp"
void Channel::update()
{
    if(events_==0)
    {
        loop_->removeChannel(this);
    }else loop_->updateChannel(this);
}
void Channel::remove()
{
    loop_->removeChannel(this);
}
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
void Channel::tie(const std::shared_ptr<void>& cb)
{
    tied_=true;
    tie_=cb;
}

