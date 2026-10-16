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
#include "TimeStamp.hpp"
#include"timer.hpp"
#include "EventLoop.hpp"
#include"EventLoopThreadPool.hpp"
#include "TcpConnnction.hpp"
// =====新增=====
#include "BusinessThreadPool.hpp"
#include "RedisClient.hpp"
#include "log.hpp"

/**
 * @brief TcpServer构造
 * @param loop 主线程EventLoop（baseLoop）
 */
Tcpserver::Tcpserver(EventLoop* loop):
baseloop_(loop)
,acceptor_(std::make_unique<Acceptor>(baseloop_))
,threadpool_(std::make_shared<EventLoopThreadPool>(baseloop_))
,businessPool_(std::make_unique<BusinessThreadPool>(4)) // =====新增===== 默认4个业务线程
,redisClient_(std::make_unique<RedisClient>())            // =====新增=====
,timer_(std::make_unique<Timer>(baseloop_))
,started_(0)
,timeoutSec_(30) // =====新增===== 默认30s空闲超时
{
	// 注册accept收到新连接回调
	acceptor_->SetNewConnctionCallback(std::bind(&Tcpserver::NewConnction,this,
    std::placeholders::_1,std::placeholders::_2));
    // 注册定时器超时回调
    timer_->SettimeoutCallback(std::bind(&Tcpserver::handleTimeout,this));
}

Tcpserver::~Tcpserver()
{
	// 销毁全部连接
	for(auto& conntion:connctions_)
    {
        TcpConnctionptr conn(conntion.second);
        conntion.second.reset();
        conn->getloop()->runInLoop(std::bind(&TcpConnction::conntDestoy,conn));
    }
    businessPool_->stop();  // =====新增=====
}

/// 设置IO线程池线程数量
void Tcpserver::SetThreadnum(int threadnum) { threadpool_->Setnumthread(threadnum); }
// =====新增===== 重置业务线程池
void Tcpserver::SetBusinessThreadNum(int num) {
    businessPool_.reset(new BusinessThreadPool(num));
}
/// 修改定时器轮询间隔
void Tcpserver::updatetimeoutPolling(int timeout)
{
	baseloop_->runInLoop(std::bind(&Timer::updatetimeoutPolling,timer_.get(),timeout));
}

/**
 * @brief 启动服务，只允许启动一次
 * 启动业务线程池、连接redis、IO线程池、开始listen监听
 */
void Tcpserver::start()
{
    if(started_++==0)
    {
        businessPool_->start();  // =====新增=====
        if (!redisClient_->connect()) {  // =====新增===== 连 Redis
            Logmanage(ERR, "redis not connected, timeout disabled");
        }
        threadpool_->start(threadInitCallback_);
        baseloop_->runInLoop(std::bind(&Acceptor::listen,acceptor_.get()));
    }
}

/**
 * @brief 外部接口：安全删除连接，投递到baseloop执行
 */
void Tcpserver::removeConnction(const TcpConnctionptr& conn)
{
    baseloop_->runInLoop(std::bind(&Tcpserver::removeInloopConnction,this,conn));
}

/**
 * @brief 在主线程loop内，从map删除连接对象
 */
void Tcpserver::removeInloopConnction(const TcpConnctionptr& conn)
{
    Logmanage(NORMOL,"conntion name:%s,fail",conn->name().c_str());
	EventLoop* ioloop=conn->getloop();
	ioloop->runInLoop(std::bind(&TcpConnction::conntDestoy,conn));
    connctions_.erase(conn->name());
   	conntionCallback_(conn);
}

/**
 * @brief 定时任务：从Redis查询空闲超时连接，关闭连接
 * 不再遍历内存map全部连接，使用Redis ZSET做超时管理，适合分布式多进程场景
 */
void Tcpserver::handleTimeout()
{
    std::vector<std::string> timeoutNames;
    if (!redisClient_->getTimeoutConns(TimeStamp::now(), timeoutSec_, timeoutNames)) {
        Logmanage(ERR, "redis getTimeoutConns failed");
        return;
    }
    if (timeoutNames.empty()) return;

    std::vector<TcpConnctionptr> totimes;
    {
        std::unique_lock<std::mutex> lock(mtx_);
        for (const auto& name : timeoutNames)
        {
            auto it = connctions_.find(name);
            if (it != connctions_.end()) totimes.push_back(it->second);
            redisClient_->removeConn(name);  // =====新增===== Redis 里清掉
        }
    }
    for (auto& conn : totimes) {
        Logmanage(NORMOL, "timeout by redis: %s", conn->name().c_str());
        removeConnction(conn);  
    }
}

/**
 * @brief Acceptor回调，新TCP连接到来
 * 从IO线程池选下一个reactor线程，创建TcpConnection对象，注入业务池、redis
 */
void Tcpserver::NewConnction(int connfd,const sockaddr_in& peer)
{
    EventLoop* ioloop=threadpool_->getNextLoop();
    std::string connName;
	connName+=inet_ntoa(peer.sin_addr);
    connName+=":";
    connName+=std::to_string(ntohs(peer.sin_port));
    TcpConnctionptr conn=(std::make_shared<TcpConnction>(connfd,ioloop,connName));
    conn->SetConntionCallback(conntionCallback_);
    conn->SetMessageCallback(messageCallback_);
    conn->SetCloseCallback(std::bind(&Tcpserver::removeConnction,this,std::placeholders::_1));
    // =====新增===== 注入业务线程池和redis客户端到连接对象
    conn->setBusinessPool(businessPool_.get());
    conn->setRedisClient(redisClient_.get());
    connctions_[connName]=conn;
   	ioloop->queueInLoop(std::bind(&TcpConnction::conntEstablish,conn));
}
