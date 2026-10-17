#pragma once
#include "Buffer.hpp"
#include"Callback.hpp"
#include "TimeStamp.hpp"
#include <atomic>
#include <ctime>
#include <memory>
class Channel;
class Socket;
class EventLoop;
class BusinessThreadPool;
class RedisClient;

/** @brief TCP 连接：fd + Channel + 收发 Buffer，消息回调在业务线程池执行 */
class TcpConnction:public std::enable_shared_from_this<TcpConnction>
{
    enum State{kDisconnected,kConnecting,kConnected,kDisConnecting};
public:
/** @brief 构造连接：封装 fd + Channel，注册读事件回调 */
	explicit TcpConnction(int fd,EventLoop* loop,std::string name);
/** @brief 析构：移除 Channel 并关闭 fd */
    ~TcpConnction();
    std::string name()
    {
        return name_;
    }
    // 注入业务线程池和 Redis 客户端（由 TcpServer 统一设置）
    void setBusinessPool(BusinessThreadPool* pool) { businessPool_ = pool; }
    void setRedisClient(RedisClient* redis) { redisClient_ = redis; }

/** @brief 设置连接建立回调（TcpServer 注入） */
    void SetConntionCallback(ConntionCallback& cb)
    {
		conntionCallback_=cb;
    }
/** @brief 设置关闭回调（TcpServer 注入） */
    void SetCloseCallback(CloseCallback&& cb)
    {
        closeCallback_=std::move(cb);
    }
/** @brief 设置消息回调（业务处理入口，TcpServer 注入） */
    void SetMessageCallback(MessageCallback& cb)
    {
        messageCallback_=cb;
    }
    bool conncted()//是否在连接状态
    {
        return state_==kConnected;
    }
/** @brief 返回连接 socket fd */
    int fd();
/** @brief 返回连接所属事件循环 */
    EventLoop* getloop();
/** @brief 连接建立：触发连接建立回调（上层可立即回包） */
    void conntEstablish();//连接建立
/** @brief 连接销毁：触发关闭回调并清理（弱引用防悬垂） */
    void conntDestoy();//连接销毁
/** @brief 发送数据（线程安全）：跨线程自动转 queueInLoop 到 IO 线程 */
	void send(const std::string& msg);//给我要发送的数据接口
private:
/** @brief IO 线程内发送：优先直写，写不完挂 write 事件 */
	void sendloop(const char* data,int len);//在自己所在的loop线程中发送数据
/** @brief 可读事件处理：读入 Buffer，按读回调分发（业务消息入口） */
	void handleRead(TimeStamp ti);//给channel的读回调函数
/** @brief 可写事件处理：继续发送剩余缓冲数据 */
    void handleWrite();//给channel的写回调函数
/** @brief 对端关闭处理：触发关闭回调并清理连接 */
    void handleClose();//给channel的断开回调函数
/** @brief 异常处理：打印错误并走关闭流程 */
    void handleErr();//给channel的异常回调函数
	EventLoop* loop_;//自己所在那个loop线程中
    std::string name_;
    BusinessThreadPool* businessPool_;//业务线程池（TcpServer 注入）
    RedisClient* redisClient_;//Redis 客户端（TcpServer 注入）
    std::unique_ptr<Socket> connsock_;//管理自己的fd性质
    std::unique_ptr<Channel> connchannel_;//该conntion对应的channel
    std::atomic_int state_;//自己的连接状态
    ConntionCallback conntionCallback_;//tcpserver给的连接回调
    CloseCallback closeCallback_;//tcpserver给的断开回调
    MessageCallback messageCallback_;//tcpserver给的业务逻辑处理回调
	Buffer inbuffer_;//该conntion自己的接收缓冲区
    Buffer outbuffer_;//该conntion自己的发送缓冲区
};
