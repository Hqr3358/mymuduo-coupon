#pragma once
#include <hiredis/hiredis.h>
#include <mutex>
#include <string>
#include <vector>
#include <ctime>

/** @brief Redis 同步客户端：ZSET 统计每个连接最后活跃时间（key=conn:last:active，member=连接名，score=时间戳）。
 *   线程安全：所有命令持内部互斥锁（hiredis context 非线程安全，IO 线程与定时线程并发调用）。
 */
class RedisClient
{
public:
    // 构造
    RedisClient();
    // 析构：断开 Redis
    ~RedisClient();

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    // 连接 Redis（主线程预连测试）
    bool connect(const std::string& host = "127.0.0.1", int port = 6379);
    // 断开 Redis
    void disconnect();

    // 更新连接活跃时间（ZADD，加锁）
    bool updateActiveTime(const std::string& connName, time_t now);

    // 取超时未活跃连接（ZRANGEBYSCORE，加锁）
    bool getTimeoutConns(time_t now, time_t timeoutSec, std::vector<std::string>& out);

    // 连接断开移除（ZREM，加锁）
    bool removeConn(const std::string& connName);

private:
    redisContext* ctx_;
    std::string key_;
    std::mutex mtx_; // 新增：串行化所有 redisCommand（hiredis context 非线程安全）
};
