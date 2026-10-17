#pragma once
#include <netinet/in.h>
#include <cstdint> // 新增：支持 uint16_t 端口参数
#include <vector>
#include <unistd.h>

/** @brief 一次 accept 的结果：fd + 对端地址 */
struct AcceptedConn
{
    int fd;
    sockaddr_in peer;
};

/** @brief socket 封装：监听、批量 accept、端口绑定与 TCP 选项设置 */
class Socket
{
public:
	Socket(int sockfd):sockfd_(sockfd){}
	~Socket(){close(sockfd_);}
	
	// 监听 socket
	void Listen();
	// 批量 accept 新连接
	void Accept(std::vector<AcceptedConn>& conns);
	// 绑定监听端口
	void BindAddress(uint16_t port = 8081);
	int fd(){return sockfd_;}
	void SetTcpNoDelay(bool on);//取消tcp延迟发送
	void SetReuseAddr(bool on);//多个进程可以绑定一个端口
	void SetReusePort(bool on);//time_wait状态下也可以绑定
	void SetKeepAlive(bool on);//客户端长时间没有通信断开
private:
	const int sockfd_;//监听fd
};