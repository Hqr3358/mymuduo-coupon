#pragma once
#include "EventLoop.hpp"
#include"Socket.hpp"
#include"Channel.hpp"
#include "TimeStamp.hpp"
#include<functional>
#include <netinet/in.h>
class Acceptor
{
    using NewConnctionCallback=std::function<void(int,const sockaddr_in&)>;
public:
	/**
	 * @brief 新增：新增端口参数（配合 Socket::BindAddress 可配置端口，默认 8081）
	 */
	Acceptor(EventLoop*loop, uint16_t port = 8081);
    ~Acceptor();
    void SetNewConnctionCallback(NewConnctionCallback&& cb)
    {
        newConnctionCallback_=std::move(cb);
    }
    void listen();
private:
	void handleRecv(TimeStamp& ti);//注册在loop中的读回调进行accept获取fd和执行newConnctionCallback_
	EventLoop* baseloop_;
	Socket accepsock_;
    Channel accepchannel_;
    bool listening_;
    NewConnctionCallback newConnctionCallback_;
};