#pragma once
#include "Buffer.hpp"
#include"Callback.hpp"
#include "TimeStamp.hpp"
#include <atomic>
#include <ctime>
#include <memory>

// =====新增===== 前置声明，减少头文件依赖
class Channel;
class Socket;
class EventLoop;
class BusinessThreadPool;
class RedisClient;

/**
 * @brief Tcp连接类，管理一条tcp连接生命周期
 * 封装socket fd、channel、读写buffer，事件回调
 */
class TcpConnction:public std::enable_shared_from_this<TcpConnction>
{
    // 连接状态枚举
    enum State{kDisconnected,kConnecting,kConnected,kDisConnecting};
public:
	explicit TcpConnction(int fd,EventLoop* loop,std::string name);
    ~TcpConnction();
    std::string name() { return name_; }

    // =====新增===== 注入业务线程池，由TcpServer创建连接时设置
    void setBusinessPool(BusinessThreadPool* pool) { businessPool_ = pool; }
    // =====新增===== 注入redis客户端句柄
    void setRedisClient(RedisClient* redis) { redisClient_ = redis; }

    void SetConntionCallback(ConntionCallback& cb) { conntionCallback_=cb; }
    void SetCloseCallback(CloseCallback&& cb) { closeCallback_=std::move(cb); }
    void SetMessageCallback(MessageCallback& cb) { messageCallback_=cb; }
    bool conncted() { return state_==kConnected; }
    int fd();
    EventLoop* getloop();
    /// 连接建立完成，注册读事件，触发连接回调
    void conntEstablish();
    /// 销毁连接，从epoll移除channel
	void conntDestoy();
    /// 对外发送消息接口，跨线程安全
	void send(const std::string& msg);
    /// 优雅关闭写端
    void shutdown();
private:
	/// 在当前IO线程内执行发送逻辑
	void sendloop(const char* data,int len);
    void shutdownInloop();
	/// 读事件回调：收到客户端数据
	void handleRead(TimeStamp ti);
    /// 写事件回调：可写，发送outbuffer缓存数据
    void handleWrite();
    /// 连接关闭回调
    void handleClose();
    /// 套接字错误回调
    void handleErr();

	EventLoop* loop_;                    /// 本连接所属IO线程的EventLoop
    std::string name_;                   /// 连接唯一标识：ip:port
    BusinessThreadPool* businessPool_;   // =====新增===== 业务线程池指针，不持有所有权
    RedisClient* redisClient_;           // =====新增===== redis客户端指针，不持有所有权
    std::unique_ptr<Socket> connsock_;
    std::unique_ptr<Channel> connchannel_;
    std::atomic_int state_;              /// 连接状态，原子变量
    ConntionCallback conntionCallback_;  /// 连接建立/断开回调
    CloseCallback closeCallback_;        /// 连接关闭回调
    MessageCallback messageCallback_;     /// 收到消息业务回调
	Buffer inbuffer_;                    /// 输入缓冲区，存放从socket读到的数据
    Buffer outbuffer_;                   /// 输出缓冲区，待发送数据
};
