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

    bool init(const std::string& host, int port);

    int claimTry(int64_t couponId, int64_t userId, long* remain);

    void claimRollback(int64_t couponId, int64_t userId);

    void claimRollbackStock(int64_t couponId);

    bool initStock(int64_t couponId, int total);

    long getStock(int64_t couponId);

    bool markUsed(const std::string& couponCode);
    bool isUsed(const std::string& couponCode);

    void clearCouponCache(int64_t couponId);

    // 供文档/展示的 Lua 脚本（与 rediscoupon.cc 内嵌一致）
    static const char* kClaimLua;

private:
    redisContext* ctx(); // thread_local 懒连接
    redisContext* rawCtx_ = nullptr; // 主线程预连句柄（init 测试用，用完即释放）
    std::string host_;
    int port_ = 6379;
    std::string sha_;            // SCRIPT LOAD 缓存的结果

    bool loadScript(redisContext* c);

    static constexpr int kUsedCacheTtlSec = 7 * 86400;
};
