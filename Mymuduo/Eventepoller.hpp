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
/** @brief epoll 封装：就绪事件轮询、Channel 增删改（Poller 具体实现） */
class Eventepoller:public Poller
{
    using EventList=std::vector<epoll_event>;
public:
	explicit Eventepoller(EventLoop* loop):Poller(loop),events_(num)
    {
        efd_=epoll_create(128);
    }
/** @brief 析构：关闭 epoll fd */
    ~Eventepoller() override;
/** @brief 【功能】epoll 等待就绪事件并填充 active 列表（事件循环核心） */
    TimeStamp poll(int timeout,ChannelList* activechannels) override;
/** @brief 从 epoll 移除 Channel */
	void removeChannel(Channel* channel) override;//在epoll中删除channel
/** @brief 更新 Channel 的 epoll 监听（ADD/MOD） */
    void updateChannel(Channel* channel) override;//在epoll中增加或更新channel
/** @brief 把就绪事件映射回 Channel 并加入 active 列表 */
    void fillActiveChannels(int numevents,ChannelList* activechannels);//填充loop的channel就绪事件队列
    private:
/** @brief 底层 epoll_ctl 封装（EPOLL_CTL_ADD/MOD/DEL） */
	void update(uint32_t option,Channel* channel);
    int efd_;
    EventList events_;
};