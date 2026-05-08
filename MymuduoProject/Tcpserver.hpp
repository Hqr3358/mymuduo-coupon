#pragma once
#include"Callback.hpp"
#include "TcpConnnction.hpp"
#include"EventLoop.hpp"
#include"Callback.hpp"

#include <atomic>
#include <functional>
#include<memory>
#include <netinet/in.h>
#include <unordered_map>
class Acceptor;
class EventLoopThreadPool;
class EventLoop;
class Timer;
class Tcpserver
{
    using ThreadInitCallback=std::function<void(EventLoop*)>;
public:
	explicit Tcpserver(EventLoop* loop);
    ~Tcpserver();
    void SetConntionCallback(ConntionCallback&& cb){conntionCallback_=std::move(cb);}
    //void SetCloseCallback(const CloseCallback &cb){closeCallback_=std::move(cb);}
    void SetMessageCallback( MessageCallback&& cb){messageCallback_=std::move(cb);}
    void SetThreadInitCallback(ThreadInitCallback&& cb){threadInitCallback_=std::move(cb);}
	void SetThreadnum(int threadnum);
    void removeConnction(const TcpConnctionptr& conn);
    void updatetimeoutPolling(int timeout);
    void start();
private:
	void NewConnction(int connfd,const sockaddr_in& peer);//来连接accept之后对connfd封装和new tcpconntion初始化
	void handleTimeout();
    void removeInloopConnction(const TcpConnctionptr& conn);
    EventLoop* baseloop_;
	std::unique_ptr<Acceptor> acceptor_;
	std::shared_ptr<EventLoopThreadPool> threadpool_;
    std::unordered_map<std::string,TcpConnctionptr> connctions_;
    std::unique_ptr<Timer> timer_;
    std::atomic_int started_;
    ConntionCallback conntionCallback_;//建立连接时的回调
    //CloseCallback closeCallback_;//断开连接时的回调
    MessageCallback messageCallback_;//读回调函数
    ThreadInitCallback threadInitCallback_;//loopthreadpool中中线程初始化时的回调
    std::mutex mtx_;
};