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
#include "log.hpp"
const int timeout=30;
Tcpserver::Tcpserver(EventLoop* loop):
baseloop_(loop)
,acceptor_(std::make_unique<Acceptor>(baseloop_))
,threadpool_(std::make_shared<EventLoopThreadPool>(baseloop_))
,timer_(std::make_unique<Timer>(baseloop_))
,started_(0)
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
}
void Tcpserver::SetThreadnum(int threadnum)
{
    threadpool_->Setnumthread(threadnum);
}
void Tcpserver::updatetimeoutPolling(int timeout)
{
	baseloop_->runInLoop(std::bind(&Timer::updatetimeoutPolling,timer_.get(),timeout));
}
void Tcpserver::start()
{
    if(started_++==0)//防止多次start
    {
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
    time_t now=TimeStamp::now();
    std::vector<TcpConnctionptr> totimes;
    {
        std::unique_lock<std::mutex> lock(mtx_);
        for(auto it=connctions_.begin();it!=connctions_.end();it++)
        {
			totimes.push_back(it->second);
        }
    }
	for(auto& totime:totimes)
    {
        TcpConnctionptr conn=totime;
		if(now-conn->gettimeout()>timeout)
        {
            Logmanage(NORMOL,"connction timeout fd:%d",conn->fd());
            removeConnction(conn);
        }
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
    connctions_[connName]=conn;
   	ioloop->queueInLoop(std::bind(&TcpConnction::conntEstablish,conn));
}
