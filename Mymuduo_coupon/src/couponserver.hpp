#pragma once
#include "Tcpserver.hpp"
#include "codec.hpp"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

/**
 * @file couponserver.hpp
 * @brief 优惠券服务器：封装自研 Mymuduo 的 Tcpserver
 *
 *   - 连接建立：为连接创建 per-connection Codec（累积缓冲切包）
 *   - 消息到达：库在业务线程池中回调 onMessage，喂给 Codec；
 *     每个完整包解析出 JSON，交给 CouponService::dispatch 处理
 *   - 连接断开：删除 Codec，释放累积状态
 *
 * 线程安全：codecs_ 表可能被 IO 线程（连接回调）与业务线程（消息回调）
 * 同时读写，用 mtx_ 保护。
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
