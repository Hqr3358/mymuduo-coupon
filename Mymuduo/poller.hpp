#pragma once
#include"Channel.hpp"
#include "TimeStamp.hpp"
#include<unordered_map>
#include <vector>
class EventLoop;
class Poller
{
    using Channelmap=std::unordered_map<int,Channel*>;
public:
	using ChannelList=std::vector<Channel*>;
	explicit Poller(EventLoop *loop):loop_(loop){}
    virtual ~Poller();
    virtual void updateChannel(Channel* channel)=0;
    virtual void removeChannel(Channel* channel)=0;
    virtual TimeStamp poll(int timeout,ChannelList* activechannels)=0;
    static  Poller* Newepoller(EventLoop *loop);
protected:
	Channelmap channelmap_;//epoll里监听的channel
private:
	EventLoop* loop_;
};