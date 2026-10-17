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
/**
 * @brief 新增：批量 accept，每个 fd 携带自己的 peer 地址
 */
void Socket::Accept(std::vector<AcceptedConn>& conns)
{
    sockaddr_in peer;
    socklen_t len = sizeof(peer);
    int savedErrno;
    while (true)
    {
        int fd = accept4(sockfd_, (sockaddr*)&peer, &len, SOCK_NONBLOCK);
        savedErrno = errno;
        if (fd > 0)
        {
            // 新增：修复：原来所有 fd 共用最后一次 accept 的 peer，
            // 导致同端口名连接覆盖；现在每个 fd 保存自己 accept 时的对端地址
            conns.push_back({fd, peer});        }
        else
        {
            if (savedErrno == EINTR) continue;
            else if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK) break;
            else
            {
                Logmanage(ERR, "accept4 failed");
                break;
            }
        }
    }
}
/**
 * @brief 新增：绑定监听端口（原实现写死 8081，改为可配置端口）
 */
void Socket::BindAddress(uint16_t port)
{
    sockaddr_in localaddr;
    memset(&(localaddr), 0,sizeof(localaddr));
    localaddr.sin_family=AF_INET;
    localaddr.sin_port=htons(port);
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