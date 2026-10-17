#pragma once
#include <hiredis/hiredis.h>
#include <mutex>
#include <string>
#include <vector>
#include <ctime>

/**
 * @brief Redis 同步客户端：用于统计每个连接的最后活跃时间。
 *        数据结构：ZSET，key = conn:last:active，member = 连接名，
 *        score = 最后活跃 Unix 时间戳（秒）。
 *   - 连接建立/每次收到消息：ZADD conn:last:active <now> <connName>
 *   - 定时扫描超时连接：ZRANGEBYSCORE conn:last:active -inf <now-timeout>
 *   - 连接断开：ZREM conn:last:active <connName>
 * @note 线程安全（重要）新增：本类被所有 sub-reactor IO 线程（handleRead 里 ZADD）
 *       与 baseloop 定时器（ZRANGEBYSCORE/ZREM）并发调用，而 hiredis 的 redisContext
 *       非线程安全（实测高并发下无锁导致偶发丢请求、服务假死）。因此每个命令调用
 *       都持有内部互斥锁，保证并发正确性。
 */
class RedisClient
{
public:
    RedisClient();
    ~RedisClient();

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    /**
     * @brief 新增：连接 Redis（默认 127.0.0.1:6379），成功返回 true
     */
    bool connect(const std::string& host = "127.0.0.1", int port = 6379);
    void disconnect();

    /**
     * @brief 新增：更新连接活跃时间（ZADD key score member），加锁
     */
    bool updateActiveTime(const std::string& connName, time_t now);

    /**
     * @brief 新增：取出超时未活跃的连接名列表（ZRANGEBYSCORE key -inf max），加锁
     */
    bool getTimeoutConns(time_t now, time_t timeoutSec, std::vector<std::string>& out);

    /**
     * @brief 新增：连接断开时从 ZSET 移除（ZREM key member），加锁
     */
    bool removeConn(const std::string& connName);

private:
    redisContext* ctx_;
    std::string key_;
    std::mutex mtx_; // 新增：串行化所有 redisCommand（hiredis context 非线程安全）
};
