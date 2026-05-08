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
Acceptor::Acceptor(EventLoop*loop):
baseloop_(loop)
,accepsock_(createNonblock())
,accepchannel_(baseloop_,accepsock_.fd())
,listening_(false)
{
    accepsock_.SetReuseAddr(true);
    accepsock_.SetReusePort(true);
    accepsock_.BindAddress();
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
void Acceptor::handleRecv(TimeStamp& ti)
{
    sockaddr_in peer;
    std::vector<int> connfds;
	accepsock_.Accept(peer,connfds);
	if(!connfds.empty())
    {
        if(newConnctionCallback_)
        {	for(int connfd:connfds)
				newConnctionCallback_(connfd,peer);
        }
        else
        {
            Logmanage(ERR,"accepor newcallback empty");
            for(int connfd:connfds) close(connfd);
        }
    }
}
