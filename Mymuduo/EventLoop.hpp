#pragma once
#include"Channel.hpp"
#include "TimeStamp.hpp"
#include "poller.hpp"
#include <atomic>
#include<mutex>
#include <memory>
#include <sys/types.h>
#include<vector>
#include<functional>
//事件循环管理channel和epoll类
class EventLoop
{
	using Functor=std::function<void()>;
public:
	EventLoop();
	~EventLoop();
	void loop();//开启循环事件
	void quit();
	void runInLoop(Functor cb);//在本身loop中直接执行，不在就queueInLoop
	void queueInLoop(Functor cb);//放入回调队列中并唤醒对应的loop执行回调
	void updateChannel(Channel* channel);//在epoll中更新channel的就绪事件
	void removeChannel(Channel* channel);//在epoll中移除对channel监听
	void weakup();//唤醒该线程－》给读事件
	bool isInLoopThread() const;//判断调用该函数时是否在本身线程
private:
	void handleRead();//weakup读事件仅用来唤醒不用填充操作
	void doPendingFunctors();//处理并执行回调函数
	std::atomic_bool looping_;
	std::atomic_bool quit_;
	TimeStamp pollReturnTime_;//poll返回时的时间点
	std::unique_ptr<Poller> poller_;//loop里的epoll
	int weakupfd_;
	std::unique_ptr<Channel> weakupchannel_;//epoll监听的channel封装的weakupfd_
	pid_t threadId_;
	std::vector<Channel*> activechannels_;//epoll返回的就绪channel
	std::atomic_bool callingpending_;
	std::vector<Functor> pendingFunctors_;//要执行的回调函数队列
	std::mutex mtx_;
};