#include "RedisClient.hpp"
#include "log.hpp"
#include <cstdio>
#include <cstring>

RedisClient::RedisClient()
    : ctx_(nullptr)
    , key_("conn:last:active")
{
}

RedisClient::~RedisClient()
{
    disconnect();
}

bool RedisClient::connect(const std::string& host, int port)
{
    ctx_ = redisConnect(host.c_str(), port);
    if (ctx_ == nullptr || ctx_->err)
    {
        Logmanage(ERR, "redis connect failed: %s", ctx_ ? ctx_->errstr : "null ctx");
        return false;
    }
    Logmanage(NORMOL, "redis connected %s:%d", host.c_str(), port);
    return true;
}

void RedisClient::disconnect()
{
    if (ctx_)
    {
        redisFree(ctx_);
        ctx_ = nullptr;
    }
}

bool RedisClient::updateActiveTime(const std::string& connName, time_t now)
{
    if (!ctx_) return false;
    redisReply* r = (redisReply*)redisCommand(ctx_, "ZADD %s %ld %s",
                                              key_.c_str(), (long)now, connName.c_str());
    if (r == nullptr)
    {
        Logmanage(ERR, "redis ZADD failed: %s", ctx_->errstr);
        return false;
    }
    freeReplyObject(r);
    return true;
}

bool RedisClient::getTimeoutConns(time_t now, time_t timeoutSec, std::vector<std::string>& out)
{
    if (!ctx_) return false;
    long maxScore = (long)(now - timeoutSec);
    redisReply* r = (redisReply*)redisCommand(ctx_, "ZRANGEBYSCORE %s -inf %ld",
                                              key_.c_str(), maxScore);
    if (r == nullptr)
    {
        Logmanage(ERR, "redis ZRANGEBYSCORE failed: %s", ctx_->errstr);
        return false;
    }
    if (r->type == REDIS_REPLY_ARRAY)
    {
        for (size_t i = 0; i < r->elements; ++i)
        {
            if (r->element[i]->type == REDIS_REPLY_STRING)
            {
                out.emplace_back(r->element[i]->str);
            }
        }
    }
    freeReplyObject(r);
    return true;
}

bool RedisClient::removeConn(const std::string& connName)
{
    if (!ctx_) return false;
    redisReply* r = (redisReply*)redisCommand(ctx_, "ZREM %s %s",
                                              key_.c_str(), connName.c_str());
    if (r == nullptr)
    {
        Logmanage(ERR, "redis ZREM failed: %s", ctx_->errstr);
        return false;
    }
    freeReplyObject(r);
    return true;
}
