#pragma once
#include <hiredis/hiredis.h>
#include <string>
#include <vector>
#include <ctime>

// Redis 同步客户端：用于统计每个连接的最后活跃时间。
// 数据结构：ZSET，key = conn:last:active，member = 连接名，score = 最后活跃 Unix 时间戳（秒）。
//   - 连接建立/每次收到消息：ZADD conn:last:active <now> <connName>
//   - 定时扫描超时连接：ZRANGEBYSCORE conn:last:active -inf <now-timeout>
//   - 连接断开：ZREM conn:last:active <connName>
class RedisClient
{
public:
    RedisClient();
    ~RedisClient();

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    // 连接 Redis，默认 127.0.0.1:6379
    bool connect(const std::string& host = "127.0.0.1", int port = 6379);
    void disconnect();

    // 更新连接活跃时间：ZADD key score member
    bool updateActiveTime(const std::string& connName, time_t now);

    // 取出超时未活跃的连接名列表：ZRANGEBYSCORE key -inf max
    bool getTimeoutConns(time_t now, time_t timeoutSec, std::vector<std::string>& out);

    // 连接断开时从 ZSET 移除：ZREM key member
    bool removeConn(const std::string& connName);

private:
    redisContext* ctx_;
    std::string key_;
};
