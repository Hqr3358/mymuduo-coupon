#pragma once
#include "TcpConnnction.hpp"
#include "mysqlpool.hpp"
#include "protocol.hpp"
#include "rediscoupon.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <unordered_map>

using json = nlohmann::json;

/**
 * @file couponservice.hpp
 * @brief 优惠券业务服务（仿 Mymuduo_chat 的 chatservice：单例 + msgid 注册表）
 *
 * 线程模型：消息回调在库的业务线程池线程中执行，本类的方法会被多个
 * 业务线程并发调用。MysqlPool（连接池 + RAII）与 CouponRedis
 * （thread_local 连接 + Lua 原子脚本）均为并发安全设计，无需额外加锁。
 *
 * 一致性设计：
 *   领券  = Redis Lua（DECR 库存 + SADD 去重，原子） + MySQL 唯一索引兜底
 *          MySQL 落库失败时回滚 Redis 预扣（INCR + SREM 补偿）
 *   核销  = MySQL 原子 UPDATE（受影响行数判定，天然幂等） + Redis 快速缓存
 */
class CouponService
{
public:
    using Handler = std::function<void(const TcpConnctionptr&, const json&)>;

    static CouponService* init();

    // 按 msgid 分发请求（协议解析在 CouponServer 完成）
    void dispatch(const TcpConnctionptr& conn, const json& js);

    bool ready() const { return ready_; }
    const std::string& error() const { return error_; }

    /**
     * @brief 优化：启动优惠券定时清理线程（后台线程，周期执行 cleanExpired）
     *   服务 ready 后调用一次即可；线程随进程退出终止。
     */
    void startCleaner();

private:
    CouponService();

    // ---- 业务处理（实现见 couponservice.cc，均有功能级 @brief 注释）----
    /**
     * @brief 【功能】心跳探测（ping 连接）
     */
    void onPing(const TcpConnctionptr& conn, const json& js);
    /**
     * @brief 【功能】发券：券模板入库 + Redis 库存初始化
     */
    void onIssue(const TcpConnctionptr& conn, const json& js);
    /**
     * @brief 【功能】领券：Redis Lua 原子防超发/去重 + MySQL 唯一索引兜底（含 1062 幂等回补）
     */
    void onClaim(const TcpConnctionptr& conn, const json& js);
    /**
     * @brief 【功能】核销：版本号乐观锁（version 条件更新+冲突重试）+ Redis 已核销缓存双保险幂等
     */
    void onUse(const TcpConnctionptr& conn, const json& js);
    /**
     * @brief 【功能】查询我的券：user_id % 5 路由单分表查询
     */
    void onQueryMine(const TcpConnctionptr& conn, const json& js);
    /**
     * @brief 【功能】查询券模板详情（含 Redis 实时库存）
     */
    void onQuery(const TcpConnctionptr& conn, const json& js);

    /**
     * @brief 优化：单轮清理任务
     *   1) 过期未下架券模板下架（coupon.status 1->0）；
     *   2) 删除下架券的 Redis stock/claimed 缓存（防内存泄漏）；
     *   3) 将下架券在各分表中"未核销"的用户券标记为已过期(status=2)。
     */
    void cleanExpired();

    // ---- 工具 ----
    void sendResp(const TcpConnctionptr& conn, int msgid, int err,
                  const std::string& msg, const json& data = json::object());
    static std::string genCode(int64_t couponId, int64_t userId);
    static std::string nowStr();
    static std::string futureStr(int days);

    std::unordered_map<int, Handler> services_;
    MysqlPool mysql_;
    CouponRedis redis_;
    bool ready_ = false;
    std::string error_;

    // 优化：定时清理线程相关
    std::thread cleaner_;
    std::atomic<bool> cleanerRunning_{false};
    /** 优化：清理周期（秒），每轮扫描一次过期券 */
    static constexpr int kCleanIntervalSec = 60;
};
