#pragma once
#include <netinet/in.h>
#include<vector>
#include <unistd.h>
class Socket
{
public:
	Socket(int sockfd):sockfd_(sockfd){}
	~Socket(){close(sockfd_);}
	
	void Listen();
	void Accept(sockaddr_in& peer,std::vector<int>& fds);
	void BindAddress();
	int fd(){return sockfd_;}
	void SetTcpNoDelay(bool on);//取消tcp延迟发送
	void SetReuseAddr(bool on);//多个进程可以绑定一个端口
	void SetReusePort(bool on);//time_wait状态下也可以绑定
	void SetKeepAlive(bool on);//客户端长时间没有通信断开
private:
	const int sockfd_;//监听fd
};