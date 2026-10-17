#pragma once
#include "Tcpserver.hpp"
#include "codec.hpp"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

 */
class CouponServer
{
public:
    explicit CouponServer(EventLoop* loop, uint16_t port = 9081);
    ~CouponServer() = default;

    CouponServer(const CouponServer&) = delete;
    CouponServer& operator=(const CouponServer&) = delete;

    void SetBusinessThreadNum(int n); // 业务线程池线程数（DB/Redis 阻塞调用）
    void SetThreadnum(int n);         // IO 线程数（sub-reactor）
    void start();

private:
    void onConnection(const TcpConnctionptr& conn);
    void onMessage(const TcpConnctionptr& conn, Buffer* buf, TimeStamp ts);

    Tcpserver server_;
    EventLoop* loop_;
    std::mutex mtx_;
    std::unordered_map<std::string, std::shared_ptr<Codec>> codecs_;
};
