#pragma once
#include <netinet/in.h>
#include <cstdint> // 新增：支持 uint16_t 端口参数
#include <vector>
#include <unistd.h>

/**
 * @brief 新增：一次 accept 的结果 = fd + 对端地址。
 *        批量 accept 时每个 fd 携带自己的 peer，
 *        避免共用最后一次 accept 的地址导致连接名冲突/覆盖
 */
struct AcceptedConn
{
    int fd;
    sockaddr_in peer;
};

class Socket
{
public:
	Socket(int sockfd):sockfd_(sockfd){}
	~Socket(){close(sockfd_);}
	
	void Listen();
	/**
	 * @brief 新增：批量 accept，返回 {fd,peer} 列表
	 *        （原 Accept(peer,fds) 改为返回 fd+peer 对）
	 */
	void Accept(std::vector<AcceptedConn>& conns);
	/**
	 * @brief 新增：端口可配置（默认 8081 兼容原行为，优惠券服务用 9081）
	 */
	void BindAddress(uint16_t port = 8081);
	int fd(){return sockfd_;}
	void SetTcpNoDelay(bool on);//取消tcp延迟发送
	void SetReuseAddr(bool on);//多个进程可以绑定一个端口
	void SetReusePort(bool on);//time_wait状态下也可以绑定
	void SetKeepAlive(bool on);//客户端长时间没有通信断开
private:
	const int sockfd_;//监听fd
};