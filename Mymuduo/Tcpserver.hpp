#pragma once
#include"Callback.hpp"
#include "TcpConnnction.hpp"
#include"EventLoop.hpp"
#include"Callback.hpp"

#include <atomic>
#include <cstdint> // 新增：支持 uint16_t 端口参数
#include <functional>
#include<memory>
#include <netinet/in.h>
#include <unordered_map>
class Acceptor;
class EventLoopThreadPool;
class EventLoop;
class Timer;
class BusinessThreadPool;
class RedisClient;
class Tcpserver
{
    using ThreadInitCallback=std::function<void(EventLoop*)>;
public:
	/**
	 * @brief 新增：新增端口参数（向下透传 Acceptor，默认 8081，优惠券服务用 9081）
	 */
	explicit Tcpserver(EventLoop* loop, uint16_t port = 8081);
    ~Tcpserver();
    void SetConntionCallback(ConntionCallback&& cb){conntionCallback_=std::move(cb);}
    void SetMessageCallback( MessageCallback&& cb){messageCallback_=std::move(cb);}
    void SetThreadInitCallback(ThreadInitCallback&& cb){threadInitCallback_=std::move(cb);}
	void SetThreadnum(int threadnum);
    void SetBusinessThreadNum(int num);//设置业务线程池线程数
    void removeConnction(const TcpConnctionptr& conn);
    void updatetimeoutPolling(int timeout);
    void start();
private:
	void NewConnction(int connfd,const sockaddr_in& peer);//来连接accept之后对connfd封装和new tcpconntion初始化
	void handleTimeout();//从 Redis ZSET 中扫描超时连接并断开
    void removeInloopConnction(const TcpConnctionptr& conn);
    EventLoop* baseloop_;
	std::unique_ptr<Acceptor> acceptor_;
	std::shared_ptr<EventLoopThreadPool> threadpool_;
    std::unique_ptr<BusinessThreadPool> businessPool_;//业务线程池
    std::unique_ptr<RedisClient> redisClient_;//Redis 客户端，用于超时统计
    std::unordered_map<std::string,TcpConnctionptr> connctions_;
    std::unique_ptr<Timer> timer_;
    std::atomic_int started_;
    int businessThreadNum_;//业务线程数
    time_t timeoutSec_;//超时断开秒数
    ConntionCallback conntionCallback_;//建立连接时的回调
    MessageCallback messageCallback_;//读回调函数
    ThreadInitCallback threadInitCallback_;//loopthreadpool中中线程初始化时的回调
    std::mutex mtx_;
};
