#include"Eventepoller.hpp"
#include "Channel.hpp"
#include "TimeStamp.hpp"
#include"log.hpp"
#include <cerrno>
#include <cstring>
#include <sys/epoll.h>
#include<unistd.h>
Eventepoller::~Eventepoller()
{
	close(efd_);
}
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

void Eventepoller::fillActiveChannels(int numevents,ChannelList* activechannels)
{
    for(int i=0;i<numevents;i++)
    {
        Channel* channel=static_cast<Channel*>(events_[i].data.ptr);
        channel->SetRevent(events_[i].events);
        activechannels->push_back(channel);
    }
}
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
