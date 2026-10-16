#pragma once
#include <hiredis/hiredis.h>
#include <string>
#include <vector>
#include <ctime>

// =====新增=====
/**
 * @brief Redis简易客户端，用于连接超时管理
 * 使用Redis ZSET保存每个连接的最后活跃时间
 * key: conn:last:active
 * member: 连接名称(IP:port)
 * score: 时间戳（最后一次收到消息的时间）
 */
class RedisClient
{
public:
    RedisClient();
    ~RedisClient();
    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    // =====新增===== 连接redis服务，默认本地6379
    bool connect(const std::string& host = "127.0.0.1", int port = 6379);
    // =====新增===== 断开redis连接，释放资源
    void disconnect();

    // =====新增=====
    /**
     * @brief 更新连接活跃时间戳，ZADD写入zset
     * @param connName 连接唯一名称
     * @param now 当前时间戳
     */
    bool updateActiveTime(const std::string& connName, time_t now);

    // =====新增=====
    /**
     * @brief 查询超时连接
     * @param now 当前时间戳
     * @param timeoutSec 超时阈值（秒），超过这个时间无消息判定超时
     * @param out 输出参数，存放所有超时连接名
     */
    bool getTimeoutConns(time_t now, time_t timeoutSec, std::vector<std::string>& out);

    // =====新增=====
    /**
     * @brief 删除zset中对应连接记录（连接关闭时调用）
     * @param connName 连接唯一名称
     */
    bool removeConn(const std::string& connName);

private:
    redisContext* ctx_;         // =====新增===== hiredis上下文
    std::string key_;           // =====新增===== zset的key名称
};
