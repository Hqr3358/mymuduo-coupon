#include"Eventepoller.hpp"
#include "Channel.hpp"
#include "TimeStamp.hpp"
#include"log.hpp"
#include <cerrno>
#include <cstring>
#include <sys/epoll.h>
#include<unistd.h>
/** @brief 析构：关闭 epoll fd */
Eventepoller::~Eventepoller()
{
	close(efd_);
}
/** @brief 【功能】epoll 等待就绪事件并填充 active 列表（事件循环核心） */
TimeStamp Eventepoller::poll(int timeout,ChannelList* activechannels)
{
    TimeStamp now;
    int num=events_.size();
	int n=epoll_wait(efd_,&events_[0],num,timeout);
    int tmeperrno=errno;
	if(n>0)
    {
        Logmanage(NORMOL, "events ready");
		if(n>3*events_.size()/4)
        {
            events_.resize(2*num);
        }
        fillActiveChannels(n,activechannels);
    }else if(n==0)
    {
		Logmanage(NORMOL,"epoll_wait timeout");
    }else
    {
		if(tmeperrno==EINTR)
        {
          Logmanage(ERR,"epoll_wait stop");
        }
    }
    return now;
}
/** @brief 从 epoll 移除 Channel */
void Eventepoller::removeChannel(Channel* channel)
{
    int index=channel->index();
	if(index==kAdded)
    {
        update(EPOLL_CTL_DEL,channel);
        channelmap_.erase(channel->fd());
        channel->Setindex(kDeleted);
    }
}
/** @brief 更新 Channel 的 epoll 监听（ADD/MOD） */
void Eventepoller::updateChannel(Channel* channel)
{
    int index=channel->index();
    if(index==kNew||index==kDeleted)
    {
       update(EPOLL_CTL_ADD,channel);
       channelmap_[channel->fd()]=channel;
       channel->Setindex(kAdded);
    }
    else
    {
        update(EPOLL_CTL_MOD,channel);
    }
}    

/** @brief 把就绪事件映射回 Channel 并加入 active 列表 */
void Eventepoller::fillActiveChannels(int numevents,ChannelList* activechannels)
{
    for(int i=0;i<numevents;i++)
    {
        Channel* channel=static_cast<Channel*>(events_[i].data.ptr);
        channel->SetRevent(events_[i].events);
        activechannels->push_back(channel);
    }
}
/** @brief 底层 epoll_ctl 封装（EPOLL_CTL_ADD/MOD/DEL） */
void Eventepoller::update(uint32_t option,Channel* channel)
{
    int fd=channel->fd();
    epoll_event event;
    memset(&event,0, sizeof(event));
    event.events=channel->events();
    event.data.fd=fd;
    event.data.ptr=channel;
    epoll_ctl(efd_,option ,fd,&event);
}
