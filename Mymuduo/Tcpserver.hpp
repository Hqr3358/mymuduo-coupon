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
/** @brief 服务器核心：Acceptor + IO 线程池 + 业务线程池 + 超时清理（one-loop-per-thread 模型） */
class Tcpserver
{
    using ThreadInitCallback=std::function<void(EventLoop*)>;
public:
	/**
	 * @brief 新增：新增端口参数（向下透传 Acceptor，默认 8081，优惠券服务用 9081）
	 */
	explicit Tcpserver(EventLoop* loop, uint16_t port = 8081);
/** @brief 析构：清理全部连接 */
    ~Tcpserver();
/** @brief 设置连接建立/断开回调 */
    void SetConntionCallback(ConntionCallback&& cb){conntionCallback_=std::move(cb);}
/** @brief 设置消息（读）回调：业务消息入口 */
    void SetMessageCallback( MessageCallback&& cb){messageCallback_=std::move(cb);}
/** @brief 设置 IO 线程初始化回调 */
    void SetThreadInitCallback(ThreadInitCallback&& cb){threadInitCallback_=std::move(cb);}
/** @brief 设置 IO 线程数（EventLoopThreadPool） */
	void SetThreadnum(int threadnum);
/** @brief 设置业务线程池线程数（业务回调执行线程） */
    void SetBusinessThreadNum(int num);//设置业务线程池线程数
/** @brief 移除连接：线程安全封装（跨线程转 queueInLoop） */
    void removeConnction(const TcpConnctionptr& conn);
/** @brief 设置超时轮询周期（秒），驱动 handleTimeout 周期执行 */
    void updatetimeoutPolling(int timeout);
/** @brief 启动服务：监听 + 启动线程池 + 启动超时定时器 */
    void start();
private:
/** @brief 新连接处理：建 TcpConnction、注册回调、加入 Map */
	void NewConnction(int connfd,const sockaddr_in& peer);//来连接accept之后对connfd封装和new tcpconntion初始化
/** @brief 【功能】超时清理：查询 Redis 超时连接并主动断开 */
	void handleTimeout();//从 Redis ZSET 中扫描超时连接并断开
/** @brief IO 线程内移除连接（从 Map 删除并销毁） */
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
