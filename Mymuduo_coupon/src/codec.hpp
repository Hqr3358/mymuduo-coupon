#pragma once
#include <functional>
#include <mutex>
#include <string>

/**
 * @file codec.hpp
 * @brief 连接级解码器
 *
 * 自研 Mymuduo 的线程模型：IO 线程读数据后把"一次 read 的完整数据"
 * 投递到业务线程池执行消息回调。同一个连接相邻两次 read 的数据可能
 * 被**不同的业务线程并发执行**回调（无连接维度顺序保证），且 TCP 存在
 * 粘包/拆包。因此解码器必须：
 *   1. 内部累积缓冲 + 互斥锁，把并发回调串行化，保证"先到先解析"；
 *   2. 按 4 字节长度头切包，解决粘包/拆包；
 *   3. 每切出一个完整包就回调一次 onMessage（在加锁区间外调用，
 *      避免持锁执行用户业务逻辑）。
 * 一个连接持有一个 Codec 实例（由 CouponServer 管理生命周期）。
 */
class Codec
{
public:
    using MessageCallback = std::function<void(const std::string& payload)>;

    explicit Codec(MessageCallback cb);

    /**
     * @brief 线程安全：任意业务线程可调用。
     *        累积 data 并切出完整包，每切出一个包在锁外调用 onMessage
     */
    void feed(const char* data, size_t len);

    /**
     * @brief 连接断开时调用：清空累积缓冲（锁内安全）
     */
    void reset();

private:
    mutable std::mutex mtx_;
    std::string inbuf_;
    MessageCallback onMessage_;
};
