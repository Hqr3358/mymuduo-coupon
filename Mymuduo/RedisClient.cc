#include "RedisClient.hpp"
#include "log.hpp"
#include <cstdio>
#include <cstring>

RedisClient::RedisClient()
    : ctx_(nullptr)
    , key_("conn:last:active")
{
}

RedisClient::~RedisClient() { disconnect(); }

// =====新增=====
/**
 * @brief 建立redis tcp连接
 * @param host redis ip
 * @param port redis端口
 * @return 成功true，失败false
 */
bool RedisClient::connect(const std::string& host, int port)
{
    ctx_ = redisConnect(host.c_str(), port);
    if (ctx_ == nullptr || ctx_->err) {
        Logmanage(ERR, "redis connect failed: %s", ctx_ ? ctx_->errstr : "null");
        return false;
    }
    Logmanage(NORMOL, "redis connected %s:%d", host.c_str(), port);
    return true;
}

// =====新增=====
/**
 * @brief 释放redis连接资源
 */
void RedisClient::disconnect()
{
    if (ctx_) { redisFree(ctx_); ctx_ = nullptr; }
}

// =====新增=====
/**
 * @brief ZADD 更新连接的最后活跃时间
 */
bool RedisClient::updateActiveTime(const std::string& connName, time_t now)
{
    if (!ctx_) return false;
    redisReply* r = (redisReply*)redisCommand(ctx_, "ZADD %s %ld %s",
        key_.c_str(), (long)now, connName.c_str());
    if (!r) { Logmanage(ERR, "ZADD fail: %s", ctx_->errstr); return false; }
    freeReplyObject(r);
    return true;
}

// =====新增=====
/**
 * @brief ZRANGEBYSCORE 查询score小于阈值的所有连接（超时连接）
 */
bool RedisClient::getTimeoutConns(time_t now, time_t timeoutSec, std::vector<std::string>& out)
{
    if (!ctx_) return false;
    // 阈值 = 当前时间 - 超时秒数；score小于该值=长时间没更新=超时
    long maxScore = (long)(now - timeoutSec);
    redisReply* r = (redisReply*)redisCommand(ctx_, "ZRANGEBYSCORE %s -inf %ld",
        key_.c_str(), maxScore);
    if (!r) { Logmanage(ERR, "ZRANGEBYSCORE fail"); return false; }
    // 解析数组返回结果
    if (r->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < r->elements; ++i) {
            if (r->element[i]->type == REDIS_REPLY_STRING)
                out.emplace_back(r->element[i]->str);
        }
    }
    freeReplyObject(r);
    return true;
}

// =====新增=====
/**
 * @brief ZREM，连接关闭时，删除redis中该连接记录
 */
bool RedisClient::removeConn(const std::string& connName)
{
    if (!ctx_) return false;
    redisReply* r = (redisReply*)redisCommand(ctx_, "ZREM %s %s",
        key_.c_str(), connName.c_str());
    if (!r) { Logmanage(ERR, "ZREM fail"); return false; }
    freeReplyObject(r);
    return true;
}
