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
Acceptor::Acceptor(EventLoop*loop, uint16_t port): // 新增：端口参数传入 Socket::BindAddress
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
void Acceptor::handleRecv(TimeStamp& ti) // 新增：适配 Accept 返回 {fd,peer} 结构体
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
