#pragma once
#include"Callback.hpp"
#include "TcpConnnction.hpp"
#include"EventLoop.hpp"
#include <atomic>
#include <functional>
#include<memory>
#include <mutex>
#include <netinet/in.h>
#include <unordered_map>

class Acceptor;
class EventLoopThreadPool;
class EventLoop;
class Timer;
// =====新增=====
class BusinessThreadPool;
class RedisClient;

/**
 * @brief Tcp服务器主类，管理监听、连接池、IO线程池、业务线程池
 */
class Tcpserver
{
    using ThreadInitCallback=std::function<void(EventLoop*)>;
public:
	explicit Tcpserver(EventLoop* loop);
    ~Tcpserver();
    void SetConntionCallback(ConntionCallback&& cb){conntionCallback_=std::move(cb);}
    void SetMessageCallback(MessageCallback&& cb){messageCallback_=std::move(cb);}
    void SetThreadInitCallback(ThreadInitCallback&& cb){threadInitCallback_=std::move(cb);}
	void SetThreadnum(int threadnum);
    // =====新增===== 设置业务线程池线程数量
    void SetBusinessThreadNum(int num);
    void removeConnction(const TcpConnctionptr& conn);
    /// 设置超时检测定时器间隔
    void updatetimeoutPolling(int timeout);
    /// 启动服务：启动IO池、业务池、accept监听
    void start();
private:
	/// Acceptor回调：收到新连接，创建TcpConnection，分配IO线程
	void NewConnction(int connfd,const sockaddr_in& peer);
    /// 定时器回调：从redis查询超时连接，关闭空闲连接
	void handleTimeout();
    /// 在baseloop内执行连接删除，操作connctions_ map
    void removeInloopConnction(const TcpConnctionptr& conn);

    EventLoop* baseloop_;                                  /// 主loop，负责accept
	std::unique_ptr<Acceptor> acceptor_;                  /// 监听套接字，接收新连接
	std::shared_ptr<EventLoopThreadPool> threadpool_;     /// IO线程池（reactor多线程）
    std::unique_ptr<BusinessThreadPool> businessPool_;     // =====新增===== 业务线程池
    std::unique_ptr<RedisClient> redisClient_;             // =====新增===== redis客户端，管理连接超时
    std::unordered_map<std::string,TcpConnctionptr> connctions_; /// 保存所有存活连接
    std::unique_ptr<Timer> timer_;                         /// 定时器，周期检测超时
    std::atomic_int started_;                              /// 服务启动标记
    time_t timeoutSec_;                                    // =====新增===== 空闲超时阈值(秒)
    ConntionCallback conntionCallback_;
    MessageCallback messageCallback_;
    ThreadInitCallback threadInitCallback_;
    std::mutex mtx_;                                       /// 保护connctions_ map
};
