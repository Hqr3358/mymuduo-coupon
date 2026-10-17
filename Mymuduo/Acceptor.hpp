#pragma once
#include "EventLoop.hpp"
#include"Socket.hpp"
#include"Channel.hpp"
#include "TimeStamp.hpp"
#include<functional>
#include <netinet/in.h>
/** @brief 监听器：accept 新连接并回调上层（连接建立入口） */
class Acceptor
{
    using NewConnctionCallback=std::function<void(int,const sockaddr_in&)>;
public:
	// 构造监听器：绑定端口、注册可读事件
// 构造监听器：绑定端口、注册可读事件回调
Acceptor(EventLoop*loop, uint16_t port = 8081);
    // 析构：移除监听 Channel
// 析构
~Acceptor();
    void SetNewConnctionCallback(NewConnctionCallback&& cb)
    {
        newConnctionCallback_=std::move(cb);
    }
    // 开始监听
// 开始监听
void listen();
private:
	void handleRecv(TimeStamp& ti);//注册在loop中的读回调进行accept获取fd和执行newConnctionCallback_
	EventLoop* baseloop_;
	Socket accepsock_;
    Channel accepchannel_;
    bool listening_;
    NewConnctionCallback newConnctionCallback_;
};