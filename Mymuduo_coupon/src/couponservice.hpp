#pragma once
#include "TcpConnnction.hpp"
#include "mysqlpool.hpp"
#include "protocol.hpp"
#include "rediscoupon.hpp"
#include <nlohmann/json.hpp>
#include <functional>
#include <string>
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

private:
    CouponService();

    // ---- 业务处理 ----
    void onPing(const TcpConnctionptr& conn, const json& js);
    void onIssue(const TcpConnctionptr& conn, const json& js);   // 发券
    void onClaim(const TcpConnctionptr& conn, const json& js);   // 领券
    void onUse(const TcpConnctionptr& conn, const json& js);     // 核销
    void onQueryMine(const TcpConnctionptr& conn, const json& js);
    void onQuery(const TcpConnctionptr& conn, const json& js);

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
};
