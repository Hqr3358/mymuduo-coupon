#pragma once
#include <hiredis/hiredis.h>
#include <cstdint>
#include <string>

 */
class CouponRedis
{
public:
    CouponRedis() = default;
    ~CouponRedis() = default;

    CouponRedis(const CouponRedis&) = delete;
    CouponRedis& operator=(const CouponRedis&) = delete;

     */
    bool init(const std::string& host, int port);

     */
    int claimTry(int64_t couponId, int64_t userId, long* remain);

     */
    void claimRollback(int64_t couponId, int64_t userId);

    /**
     * @brief 优化：仅回补库存（INCR），不操作 claimed 集合。
     *   用于 MySQL 唯一索引冲突（用户 DB 已有领取记录）时：保持
     *   claimed 集合含该用户（保证后续走 duplicate 路径），只把
     *   Lua 预扣的库存补回。
     */
    void claimRollbackStock(int64_t couponId);

     */
    bool initStock(int64_t couponId, int total);

     */
    long getStock(int64_t couponId);

    /**
     * @brief 核销成功缓存已核销券码（快速幂等路径）
     *   优化：写缓存时设置 7 天 TTL 自动过期（kUsedCacheTtlSec），
     *   避免 coupon:used:* 无限增长；缓存过期后由 DB 原子 UPDATE 兜底，
     *   幂等语义不变。
     */
    bool markUsed(const std::string& couponCode);
     */
    bool isUsed(const std::string& couponCode);

    /**
     * @brief 优化：删除某张券的 Redis 库存/领取集合缓存
     *   （定时清理器下架过期券时调用，防残留 key 泄漏内存）
     */
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

    /** 优化：核销缓存 TTL（秒）= 7 天，到期自动过期（DB 兜底幂等） */
    static constexpr int kUsedCacheTtlSec = 7 * 86400;
};
