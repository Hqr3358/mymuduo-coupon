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

class TcpConnction:public std::enable_shared_from_this<TcpConnction>
{
    enum State{kDisconnected,kConnecting,kConnected,kDisConnecting};
public:
	explicit TcpConnction(int fd,EventLoop* loop,std::string name);
    ~TcpConnction();
    std::string name()
    {
        return name_;
    }
    // 注入业务线程池和 Redis 客户端（由 TcpServer 统一设置）
    void setBusinessPool(BusinessThreadPool* pool) { businessPool_ = pool; }
    void setRedisClient(RedisClient* redis) { redisClient_ = redis; }

    void SetConntionCallback(ConntionCallback& cb)
    {
		conntionCallback_=cb;
    }
    void SetCloseCallback(CloseCallback&& cb)
    {
        closeCallback_=std::move(cb);
    }
    void SetMessageCallback(MessageCallback& cb)
    {
        messageCallback_=cb;
    }
    bool conncted()//是否在连接状态
    {
        return state_==kConnected;
    }
    int fd();
    EventLoop* getloop();
    void conntEstablish();//连接建立
    void conntDestoy();//连接销毁
	void send(const std::string& msg);//给我要发送的数据接口
    void shutdown();//断开连接接口
private:
	void sendloop(const char* data,int len);//在自己所在的loop线程中发送数据
    void shutdownInloop();//断开连接将自己从tcpserver中的vector中去掉,和从epoller中去掉
	void handleRead(TimeStamp ti);//给channel的读回调函数
    void handleWrite();//给channel的写回调函数
    void handleClose();//给channel的断开回调函数
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
