#pragma once
#include <hiredis/hiredis.h>
#include <cstdint>
#include <string>

/** @brief 优惠券业务 Redis 客户端：Lua 脚本原子领券（防超发+去重）、库存/去重/已核销缓存 */
class CouponRedis
{
public:
    CouponRedis() = default;
    ~CouponRedis() = default;

    CouponRedis(const CouponRedis&) = delete;
    CouponRedis& operator=(const CouponRedis&) = delete;

    // 初始化 Redis 连接 + 加载 Lua 脚本
    bool init(const std::string& host, int port);

    // 功能：领券原子操作（Lua 防超发+去重）
    int claimTry(int64_t couponId, int64_t userId, long* remain);

    // 功能：领券失败回滚（回补库存+SREM）
    void claimRollback(int64_t couponId, int64_t userId);

    // 功能：1062 冲突时仅回补库存
    void claimRollbackStock(int64_t couponId);

    // 功能：发券初始化 Redis 库存（带 TTL，基于券有效期 + 随机偏移防雪崩）
    bool initStock(int64_t couponId, int total, long ttlSec);

    // 功能：查询实时库存
    long getStock(int64_t couponId);

    // ---- 券元数据缓存（Cache-Aside：发券时写入，领券时先读 Redis，miss 回查 DB 回填）----
    // 写券元数据到 Redis（HSET coupon:info:{id}），并设置 TTL
    void cacheCouponInfo(int64_t couponId, const std::string& title,
                         int status, const std::string& endTime, int total, long ttlSec);
    // 读券元数据；命中返回 true 并填充 out；未命中返回 false
    bool getCouponInfo(int64_t couponId, std::string& title, int& status,
                       std::string& endTime, int& total);
    // 券下架/过期时删元数据缓存
    void invalidateCouponInfo(int64_t couponId);
    // 券不存在时缓存空值（短 TTL 防穿透）
    void markCouponMissing(int64_t couponId);
    // 判断是否被标记为不存在
    bool isCouponMissing(int64_t couponId);

    // 功能：核销成功写已核销缓存（7天TTL）
    bool markUsed(const std::string& couponCode);
    // 功能：核销幂等前置检查
    bool isUsed(const std::string& couponCode);

    // 功能：定时清理券缓存
    void clearCouponCache(int64_t couponId);

    // 供文档/展示的 Lua 脚本（与 rediscoupon.cc 内嵌一致）
    static const char* kClaimLua;

private:
    redisContext* ctx(); // thread_local 懒连接
    redisContext* rawCtx_ = nullptr; // 主线程预连句柄（init 测试用，用完即释放）
    std::string host_;
    int port_ = 6379;
    std::string sha_;            // SCRIPT LOAD 缓存的结果

    // 加载 Lua 脚本（SCRIPT LOAD）
    bool loadScript(redisContext* c);

    static constexpr int kUsedCacheTtlSec = 7 * 86400;
};
