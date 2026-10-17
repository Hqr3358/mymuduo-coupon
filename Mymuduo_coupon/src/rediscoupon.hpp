#pragma once
#include <hiredis/hiredis.h>
#include <cstdint>
#include <string>

/**
 * @file rediscoupon.hpp
 * @brief 优惠券业务 Redis 客户端
 *
 * 线程模型：业务线程池多线程并发访问，hiredis 的 redisContext 非线程安全，
 * 因此每个业务线程持有自己的连接（thread_local 懒加载）。
 *
 * Key 设计：
 *   coupon:stock:{couponId}              STRING 剩余库存
 *   coupon:claimed:{couponId}            SET   已领取该券的用户ID（防重复领取）
 *   coupon:used:{couponCode}             STRING 已核销的券码（核销快速幂等缓存）
 *
 * 防超发 + 防重复领取由一段 Lua 脚本原子完成（Redis 单线程执行脚本，
 * 天然无竞态），MySQL 唯一索引做最终兜底。
 */
class CouponRedis
{
public:
    CouponRedis() = default;
    ~CouponRedis() = default;

    CouponRedis(const CouponRedis&) = delete;
    CouponRedis& operator=(const CouponRedis&) = delete;

    /**
     * @brief 主线程预连测试（失败返回 false，业务上可降级/拒绝启动）
     */
    bool init(const std::string& host, int port);

    /**
     * @brief 领券原子操作（Lua 脚本）
     *   成功返回 ERR_OK，*remain 为该用户领取后的剩余库存
     *   失败返回 ERR_NO_STOCK / ERR_DUP_CLAIM / ERR_NOT_EXIST / ERR_REDIS
     */
    int claimTry(int64_t couponId, int64_t userId, long* remain);

    /**
     * @brief 补偿：MySQL 落库失败时回滚 Redis 预扣（INCR 库存 + SREM 用户）
     */
    void claimRollback(int64_t couponId, int64_t userId);

    /**
     * @brief 优化：仅回补库存（INCR），不操作 claimed 集合。
     *   用于 MySQL 唯一索引冲突（用户 DB 已有领取记录）时：保持
     *   claimed 集合含该用户（保证后续走 duplicate 路径），只把
     *   Lua 预扣的库存补回。
     */
    void claimRollbackStock(int64_t couponId);

    /**
     * @brief 发券初始化库存
     */
    bool initStock(int64_t couponId, int total);

    /**
     * @brief 查询实时库存（无 key 返回 -1）
     */
    long getStock(int64_t couponId);

    /**
     * @brief 核销成功缓存已核销券码（快速幂等路径）
     *   优化：写缓存时设置 7 天 TTL 自动过期（kUsedCacheTtlSec），
     *   避免 coupon:used:* 无限增长；缓存过期后由 DB 原子 UPDATE 兜底，
     *   幂等语义不变。
     */
    bool markUsed(const std::string& couponCode);
    /**
     * @brief 快速判"已核销"（仅加速，最终以 DB 原子 UPDATE 为准）
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
    redisContext* rawCtx_;       // 主线程预连句柄（测试用）
    std::string host_;
    int port_ = 6379;
    std::string sha_;            // SCRIPT LOAD 缓存的结果
    bool scriptLoaded_ = false;

    bool loadScript(redisContext* c);

    /** 优化：核销缓存 TTL（秒）= 7 天，到期自动过期（DB 兜底幂等） */
    static constexpr int kUsedCacheTtlSec = 7 * 86400;
};
