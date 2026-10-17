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
/** @brief 事件循环（Reactor 核心）：管理 Channel 注册、epoll 就绪事件分发与跨线程任务投递 */
class EventLoop
{
	using Functor=std::function<void()>;
public:
/** @brief 构造：创建 epoll poller + eventfd 唤醒通道，绑定所属 IO 线程 */
	EventLoop();
/** @brief 析构：移除唤醒通道并关闭 eventfd */
	~EventLoop();
/** @brief 【功能】事件循环主循环：epoll_wait 阻塞 + 分发就绪事件 + 执行 pending 任务 */
	void loop();//开启循环事件
/** @brief 退出事件循环（置 quit_ 标志，唤醒后自然退出） */
	void quit();
/** @brief 在 IO 线程执行任务：本线程直接执行，跨线程转 queueInLoop */
	void runInLoop(Functor cb);//在本身loop中直接执行，不在就queueInLoop
/** @brief 把任务挂入 pending 队列并唤醒 IO 线程（跨线程安全投递） */
	void queueInLoop(Functor cb);//放入回调队列中并唤醒对应的loop执行回调
/** @brief 注册/更新 Channel 到 poller（加入事件监听） */
	void updateChannel(Channel* channel);//在epoll中更新channel的就绪事件
/** @brief 从 poller 移除 Channel（取消事件监听） */
	void removeChannel(Channel* channel);//在epoll中移除对channel监听
/** @brief 唤醒 IO 线程（eventfd 写入，使阻塞中的 poll 立即返回） */
	void weakup();//唤醒该线程－》给读事件
/** @brief 判断当前线程是否为该事件循环所属 IO 线程 */
	bool isInLoopThread() const;//判断调用该函数时是否在本身线程
private:
/** @brief 唤醒事件处理：读取并清空 eventfd 计数 */
	void handleRead();//weakup读事件仅用来唤醒不用填充操作
/** @brief 执行 pending 任务队列（跨线程投递的回调在此运行） */
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