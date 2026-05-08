#pragma once
#include "Channel.hpp"
#include "TimeStamp.hpp"
#include"poller.hpp"
#include <vector>
#include<sys/epoll.h>
const int num=1024;//muduo中是这个返回就绪队列最大长度
const int kNew=-1;//从未进过epoll里
const int kAdded=1;//现在在epoll里
const int kDeleted=2;//被epoll删除了
class Eventepoller:public Poller
{
    using EventList=std::vector<epoll_event>;
public:
	explicit Eventepoller(EventLoop* loop):Poller(loop),events_(num)
    {
        efd_=epoll_create(128);
    }
    ~Eventepoller() override;
    TimeStamp poll(int timeout,ChannelList* activechannels) override;
	void removeChannel(Channel* channel) override;//在epoll中删除channel
    void updateChannel(Channel* channel) override;//在epoll中增加或更新channel
    void fillActiveChannels(int numevents,ChannelList* activechannels);//填充loop的channel就绪事件队列
    private:
	void update(uint32_t option,Channel* channel);
    int efd_;
    EventList events_;
};