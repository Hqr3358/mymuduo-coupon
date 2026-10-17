#pragma once
#include "Tcpserver.hpp"
#include "codec.hpp"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

/** @brief 优惠券服务器：封装 Mymuduo Tcpserver，连接/消息回调绑定到 CouponService */
class CouponServer
{
public:
    // 构造服务器：绑定回调到 Tcpserver
    explicit CouponServer(EventLoop* loop, uint16_t port = 9081);
    ~CouponServer() = default;

    CouponServer(const CouponServer&) = delete;
    CouponServer& operator=(const CouponServer&) = delete;

    void SetBusinessThreadNum(int n); // 业务线程池线程数（DB/Redis 阻塞调用）
    void SetThreadnum(int n);         // IO 线程数（sub-reactor）
    // 启动服务
    void start();

private:
    // 功能：连接建立/断开回调（维护每连接解码器）
    void onConnection(const TcpConnctionptr& conn);
        // 功能：消息回调（喂解码器后分发业务）
void onMessage(const TcpConnctionptr& conn, Buffer* buf, TimeStamp ts);

    Tcpserver server_;
    EventLoop* loop_;
    std::mutex mtx_;
    std::unordered_map<std::string, std::shared_ptr<Codec>> codecs_;
};
