#include"Socket.hpp"
#include"log.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
void Socket::Listen()
{
	if(listen(sockfd_, 1024)<0)
    {
        Logmanage(FATAL,"listen fali");
    }
}
void Socket::Accept(sockaddr_in& peer,std::vector<int>& fds)
{
    socklen_t len=sizeof(peer);
    int savedErrno;
    while(true)
    {
        int fd=accept4(sockfd_, (sockaddr *)&peer,&len,SOCK_NONBLOCK);
        savedErrno=errno;
        if(fd>0)
        {
            fds.push_back(fd);
        }
        else
        {
            if(savedErrno==EINTR) continue;
            else if(savedErrno==EAGAIN||savedErrno==EWOULDBLOCK) break;
            else
            {
                Logmanage(ERR,"accept4 failed");
                break;
            }
        }
    }
}
void Socket::BindAddress()
{
    sockaddr_in localaddr;
    memset(&(localaddr), 0,sizeof(localaddr));
    localaddr.sin_family=AF_INET;
    localaddr.sin_port=htons(8081);
    localaddr.sin_addr.s_addr=INADDR_ANY;
	if(bind(sockfd_,(sockaddr*)&localaddr,sizeof(localaddr))<0)
    {
        Logmanage(FATAL,"bind fali");
    }
}
void Socket::SetTcpNoDelay(bool on)
{
    int optval=on?1:0;
    setsockopt(sockfd_,IPPROTO_TCP,TCP_NODELAY,&optval,sizeof(optval));
}
void Socket::SetReuseAddr(bool on)
{
    int optval=on?1:0;
    setsockopt(sockfd_,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval));
}
void Socket::SetReusePort(bool on)
{
    int optval=on?1:0;
    setsockopt(sockfd_,SOL_SOCKET,SO_REUSEPORT,&optval,sizeof(optval));
}
void Socket::SetKeepAlive(bool on)
{
    int optval=on?1:0;
    setsockopt(sockfd_,SOL_SOCKET,SO_KEEPALIVE,&optval,sizeof(optval));
}