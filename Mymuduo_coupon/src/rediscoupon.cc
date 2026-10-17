#include "rediscoupon.hpp"
#include "protocol.hpp"
#include "log.hpp"
#include <cstdlib>
#include <cstring>

// 领券原子脚本：
//   KEYS[1] = coupon:stock:{couponId}
//   KEYS[2] = coupon:claimed:{couponId}
//   ARGV[1] = userId
// 返回：
//   >=0 剩余库存（领取成功，已 DECR + SADD）
//   -1  库存不足
//   -2  该用户已领取（重复）
//   -3  券不存在（库存 key 未初始化，可能已下架）
const char* CouponRedis::kClaimLua =
    "local stock = redis.call('GET', KEYS[1])\n"
    "if not stock then return -3 end\n"
    "stock = tonumber(stock)\n"
    "if stock <= 0 then return -1 end\n"
    "if redis.call('SISMEMBER', KEYS[2], ARGV[1]) == 1 then return -2 end\n"
    "redis.call('DECR', KEYS[1])\n"
    "redis.call('SADD', KEYS[2], ARGV[1])\n"
    "return stock - 1\n";

// thread_local：每个业务线程一条 Redis 连接
static thread_local redisContext* t_ctx = nullptr;

bool CouponRedis::init(const std::string& host, int port)
{
    host_ = host;
    port_ = port;
    rawCtx_ = redisConnect(host.c_str(), port);
    if (!rawCtx_ || rawCtx_->err)
    {
        Logmanage(ERR, "redis init failed: %s", rawCtx_ ? rawCtx_->errstr : "null ctx");
        if (rawCtx_)
        {
            redisFree(rawCtx_);
            rawCtx_ = nullptr;
        }
        return false;
    }
    if (!loadScript(rawCtx_))
    {
        Logmanage(ERR, "redis load script failed");
        redisFree(rawCtx_);
        rawCtx_ = nullptr;
        return false;
    }
    // 优化：预连测试完成，立即释放主线程句柄（业务访问统一走 thread_local 懒连接），
    // 避免 rawCtx_ 长期持有导致连接泄漏
    redisFree(rawCtx_);
    rawCtx_ = nullptr;
    Logmanage(NORMOL, "coupon redis connected %s:%d, script loaded", host.c_str(), port);
    return true;
}

redisContext* CouponRedis::ctx()
{
    if (t_ctx) return t_ctx;
    t_ctx = redisConnect(host_.c_str(), port_);
    if (!t_ctx || t_ctx->err)
    {
        Logmanage(ERR, "redis thread-local connect failed: %s", t_ctx ? t_ctx->errstr : "null");
        if (t_ctx)
        {
            redisFree(t_ctx);
            t_ctx = nullptr;
        }
        return nullptr;
    }
    loadScript(t_ctx);
    return t_ctx;
}

bool CouponRedis::loadScript(redisContext* c)
{
    redisReply* r = (redisReply*)redisCommand(c, "SCRIPT LOAD %s", kClaimLua);
    if (!r)
    {
        Logmanage(ERR, "redis SCRIPT LOAD failed: %s", c->errstr);
        return false;
    }
    bool ok = r->type == REDIS_REPLY_STRING && r->str != nullptr;
    if (ok)
    {
        sha_ = r->str;
    }
    freeReplyObject(r);
    return ok;
}

/**
 * @brief 【功能】领券原子操作（对应功能测试第 2/3 项：防超发 + 领取去重）
 * @details 执行 claim_coupon.lua（EVALSHA，NOSCRIPT 时回退 EVAL）：
 *          单脚本内原子完成「库存检查 → DECR → SADD 用户」，Redis 单线程
 *          执行脚本无并发竞态；返回剩余库存（>=0）/ -1 无库存 / -2 已领取 /
 *          -3 券不存在。
 * @param couponId 券 ID
 * @param userId   用户 ID
 * @param remain   出参：该用户领取后的剩余库存
 * @return ERR_OK / ERR_NO_STOCK / ERR_DUP_CLAIM / ERR_NOT_EXIST / ERR_REDIS
 */
int CouponRedis::claimTry(int64_t couponId, int64_t userId, long* remain)
{
    redisContext* c = ctx();
    if (!c) return ERR_REDIS;

    std::string stockKey = "coupon:stock:" + std::to_string(couponId);
    std::string claimedKey = "coupon:claimed:" + std::to_string(couponId);
    std::string uid = std::to_string(userId);

    redisReply* r = (redisReply*)redisCommand(c, "EVALSHA %s 2 %s %s %s",
                                              sha_.c_str(),
                                              stockKey.c_str(), claimedKey.c_str(),
                                              uid.c_str());
    if (!r)
    {
        // NOSCRIPT（脚本可能被 FLUSHALL）则回退 EVAL
        if (c->err && std::strstr(c->errstr, "NOSCRIPT"))
        {
            redisFree(c);
            t_ctx = nullptr;
            c = ctx();
            if (!c) return ERR_REDIS;
            r = (redisReply*)redisCommand(c, "EVAL %s 2 %s %s %s",
                                          kClaimLua,
                                          stockKey.c_str(), claimedKey.c_str(),
                                          uid.c_str());
        }
        if (!r)
        {
            Logmanage(ERR, "redis EVALSHA failed: %s", c->errstr);
            return ERR_REDIS;
        }
    }

    if (r->type != REDIS_REPLY_INTEGER)
    {
        freeReplyObject(r);
        return ERR_REDIS;
    }

    long long val = r->integer;
    freeReplyObject(r);

    if (val >= 0)
    {
        if (remain) *remain = val;
        return ERR_OK;
    }
    if (val == -1) return ERR_NO_STOCK;
    if (val == -2) return ERR_DUP_CLAIM;
    return ERR_NOT_EXIST;
}

void CouponRedis::claimRollback(int64_t couponId, int64_t userId)
{
    redisContext* c = ctx();
    if (!c) return;

    std::string stockKey = "coupon:stock:" + std::to_string(couponId);
    std::string claimedKey = "coupon:claimed:" + std::to_string(couponId);
    std::string uid = std::to_string(userId);

    redisReply* r1 = (redisReply*)redisCommand(c, "INCR %s", stockKey.c_str());
    if (r1) freeReplyObject(r1);
    redisReply* r2 = (redisReply*)redisCommand(c, "SREM %s %s", claimedKey.c_str(), uid.c_str());
    if (r2) freeReplyObject(r2);
}

void CouponRedis::claimRollbackStock(int64_t couponId)
{
    // 优化：只回补库存，保留 claimed 集合（见头文件说明）
    redisContext* c = ctx();
    if (!c) return;

    std::string stockKey = "coupon:stock:" + std::to_string(couponId);
    redisReply* r = (redisReply*)redisCommand(c, "INCR %s", stockKey.c_str());
    if (r) freeReplyObject(r);
}

bool CouponRedis::initStock(int64_t couponId, int total)
{
    redisContext* c = ctx();
    if (!c) return false;

    std::string stockKey = "coupon:stock:" + std::to_string(couponId);
    std::string claimedKey = "coupon:claimed:" + std::to_string(couponId);

    redisReply* r = (redisReply*)redisCommand(c, "SET %s %d", stockKey.c_str(), total);
    if (r) freeReplyObject(r);
    redisReply* r2 = (redisReply*)redisCommand(c, "DEL %s", claimedKey.c_str());
    if (r2) freeReplyObject(r2);
    return true;
}

long CouponRedis::getStock(int64_t couponId)
{
    redisContext* c = ctx();
    if (!c) return -1;

    std::string stockKey = "coupon:stock:" + std::to_string(couponId);
    redisReply* r = (redisReply*)redisCommand(c, "GET %s", stockKey.c_str());
    if (!r) return -1;
    long v = -1;
    if (r->type == REDIS_REPLY_STRING && r->str)
    {
        v = std::atol(r->str);
    }
    freeReplyObject(r);
    return v;
}

bool CouponRedis::markUsed(const std::string& couponCode)
{
    redisContext* c = ctx();
    if (!c) return false;
    // 优化：SET 带 EX 7 天 TTL，缓存到期自动过期（见头文件说明）
    redisReply* r = (redisReply*)redisCommand(c, "SET coupon:used:%s 1 EX %d",
                                              couponCode.c_str(), kUsedCacheTtlSec);
    if (r)
    {
        freeReplyObject(r);
        return true;
    }
    return false;
}

bool CouponRedis::isUsed(const std::string& couponCode)
{
    redisContext* c = ctx();
    if (!c) return false;
    redisReply* r = (redisReply*)redisCommand(c, "GET coupon:used:%s", couponCode.c_str());
    if (!r) return false;
    bool used = (r->type == REDIS_REPLY_STRING);
    freeReplyObject(r);
    return used;
}

void CouponRedis::clearCouponCache(int64_t couponId)
{
    // 优化：删除券的库存/领取集合缓存（定时清理器下架过期券时调用）
    redisContext* c = ctx();
    if (!c) return;

    std::string stockKey = "coupon:stock:" + std::to_string(couponId);
    std::string claimedKey = "coupon:claimed:" + std::to_string(couponId);
    redisReply* r = (redisReply*)redisCommand(c, "DEL %s %s", stockKey.c_str(), claimedKey.c_str());
    if (r) freeReplyObject(r);
}
