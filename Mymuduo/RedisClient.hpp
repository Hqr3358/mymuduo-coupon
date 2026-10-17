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
    RedisClient();
    ~RedisClient();

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    bool connect(const std::string& host = "127.0.0.1", int port = 6379);
    void disconnect();

    bool updateActiveTime(const std::string& connName, time_t now);

    bool getTimeoutConns(time_t now, time_t timeoutSec, std::vector<std::string>& out);

    bool removeConn(const std::string& connName);

private:
    redisContext* ctx_;
    std::string key_;
    std::mutex mtx_; // 新增：串行化所有 redisCommand（hiredis context 非线程安全）
};
