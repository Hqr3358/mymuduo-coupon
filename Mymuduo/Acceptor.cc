#include"Acceptor.hpp"
#include "TimeStamp.hpp"
#include "log.hpp"
#include <netinet/in.h>
#include <sys/socket.h>
static int createNonblock()
{
    int fd=socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK,0);
    if(fd<0) Logmanage(FATAL,"creat socket fail");
    return fd;
}
/**
 * @brief 新增：构造 Acceptor，端口参数传入 Socket::BindAddress（监听端口可配置）
 */
Acceptor::Acceptor(EventLoop*loop, uint16_t port):
baseloop_(loop)
,accepsock_(createNonblock())
,accepchannel_(baseloop_,accepsock_.fd())
,listening_(false)
{
    accepsock_.SetReuseAddr(true);
    accepsock_.SetReusePort(true);
    accepsock_.BindAddress(port);
    accepchannel_.SetrecvCallback(std::bind(&Acceptor::handleRecv,this,std::placeholders::_1));
}
Acceptor::~Acceptor()
{
    accepchannel_.remove();
}

void Acceptor::listen()
{
    listening_=true;
    accepsock_.Listen();
    accepchannel_.SetEnableIn();
}
/**
 * @brief 新增：适配 Accept 返回 {fd,peer} 结构体，逐个回调 newConnctionCallback_
 */
void Acceptor::handleRecv(TimeStamp& ti)
{
    std::vector<AcceptedConn> conns;
	accepsock_.Accept(conns);
	if(!conns.empty())
    {
        if(newConnctionCallback_)
        {
            for (auto& c : conns)
                newConnctionCallback_(c.fd, c.peer);
        }
        else
        {
            Logmanage(ERR,"accepor newcallback empty");
            for (auto& c : conns) close(c.fd);
        }
    }
}
