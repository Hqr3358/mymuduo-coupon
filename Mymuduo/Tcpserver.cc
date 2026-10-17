#include"Tcpserver.hpp"
#include "Acceptor.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include"Acceptor.hpp"
#include "TimeStamp.hpp"
#include"timer.hpp"
#include "Callback.hpp"
#include "EventLoop.hpp"
#include"EventLoopThreadPool.hpp"
#include "TcpConnnction.hpp"
#include "BusinessThreadPool.hpp"
#include "RedisClient.hpp"
#include "log.hpp"

/**
 * @brief 新增：构造 TcpServer，端口透传给 Acceptor（监听端口可配置）
 */
Tcpserver::Tcpserver(EventLoop* loop, uint16_t port):
baseloop_(loop)
,acceptor_(std::make_unique<Acceptor>(baseloop_, port))
,threadpool_(std::make_shared<EventLoopThreadPool>(baseloop_))
,businessPool_(std::make_unique<BusinessThreadPool>(4))
,redisClient_(std::make_unique<RedisClient>())
,timer_(std::make_unique<Timer>(baseloop_))
,started_(0)
,businessThreadNum_(4)
,timeoutSec_(30)
{
	acceptor_->SetNewConnctionCallback(std::bind(&Tcpserver::NewConnction,this,
    std::placeholders::_1,std::placeholders::_2));
    timer_->SettimeoutCallback(std::bind(&Tcpserver::handleTimeout,this));
}
Tcpserver::~Tcpserver()
{
	for(auto& conntion:connctions_)
    {
        TcpConnctionptr conn(conntion.second);
        conntion.second.reset();
        conn->getloop()->runInLoop(std::bind(&TcpConnction::conntDestoy,conn));
    }
    businessPool_->stop();
}
void Tcpserver::SetThreadnum(int threadnum)
{
    threadpool_->Setnumthread(threadnum);
}
void Tcpserver::SetBusinessThreadNum(int num)
{
    businessThreadNum_ = num;
    businessPool_.reset(new BusinessThreadPool(num));
}
void Tcpserver::updatetimeoutPolling(int timeout)
{
	baseloop_->runInLoop(std::bind(&Timer::updatetimeoutPolling,timer_.get(),timeout));
}
void Tcpserver::start()
{
    if(started_++==0)//防止多次start
    {
        // 启动业务线程池
        businessPool_->start();
        // 连接 Redis，失败不阻塞启动，超时机制降级为不可用
        if (!redisClient_->connect())
        {
            Logmanage(ERR, "redis not connected, timeout disconnect disabled");
        }
        threadpool_->start(threadInitCallback_);
        baseloop_->runInLoop(std::bind(&Acceptor::listen,acceptor_.get()));
    }
}
void Tcpserver::removeConnction(const TcpConnctionptr& conn)
{
    baseloop_->runInLoop(std::bind(&Tcpserver::removeInloopConnction,this,conn));
}
void Tcpserver::removeInloopConnction(const TcpConnctionptr& conn)
{
    Logmanage(NORMOL,"conntion name:%s,fail",conn->name().c_str());
	EventLoop* ioloop=conn->getloop();
	ioloop->runInLoop(std::bind(&TcpConnction::conntDestoy,conn));
    connctions_.erase(conn->name());
   	conntionCallback_(conn);
}
void Tcpserver::handleTimeout()
{
    // 从 Redis ZSET 取出超时未活跃的连接名列表
    std::vector<std::string> timeoutNames;
    if (!redisClient_->getTimeoutConns(TimeStamp::now(), timeoutSec_, timeoutNames))
    {
        Logmanage(ERR, "redis getTimeoutConns failed, skip this round");
        return;
    }
    if (timeoutNames.empty()) return;

    // 拷贝当前所有连接的 shared_ptr，避免锁内操作
    std::vector<TcpConnctionptr> totimes;
    {
        std::unique_lock<std::mutex> lock(mtx_);
        for (const auto& name : timeoutNames)
        {
            auto it = connctions_.find(name);
            if (it != connctions_.end())
            {
                totimes.push_back(it->second);
            }
            // 不管在不在本地 map，都从 Redis 清理掉
            redisClient_->removeConn(name);
        }
    }
    for (auto& conn : totimes)
    {
        Logmanage(NORMOL, "connction timeout by redis, name:%s fd:%d",
                  conn->name().c_str(), conn->fd());
        removeConnction(conn);
    }
}
void Tcpserver::NewConnction(int connfd,const sockaddr_in& peer)
{
    EventLoop* ioloop=threadpool_->getNextLoop();
    std::string connName;
	connName+=inet_ntoa(peer.sin_addr);
    connName+="::";
    connName+=std::to_string(ntohs(peer.sin_port));
    TcpConnctionptr conn=(std::make_shared<TcpConnction>(connfd,ioloop,connName));
    conn->SetConntionCallback(conntionCallback_);
    conn->SetMessageCallback(messageCallback_);
    conn->SetCloseCallback(std::bind(&Tcpserver::removeConnction,this,std::placeholders::_1));
    // 注入业务线程池和 Redis 客户端
    conn->setBusinessPool(businessPool_.get());
    conn->setRedisClient(redisClient_.get());
    connctions_[connName]=conn;
   	ioloop->queueInLoop(std::bind(&TcpConnction::conntEstablish,conn));
}
