#pragma once
#include "Callback.hpp"
#include "mysqlpool.hpp"
#include "rediscoupon.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <unordered_map>

using json = nlohmann::json;

/** @brief 优惠券业务服务（单例 + msgid 注册表）：领券=Redis Lua 原子防超发+MySQL 唯一索引兜底；核销=版本号乐观锁幂等 */
class CouponService
{
public:
    using Handler = std::function<void(const TcpConnctionptr&, const json&)>;

    static CouponService* init();

    // 按 msgid 分发请求（协议解析在 CouponServer 完成）
    void dispatch(const TcpConnctionptr& conn, const json& js);

    bool ready() const { return ready_; }
    const std::string& error() const { return error_; }

    void startCleaner();

private:
    CouponService();

    // ---- 业务处理（实现见 couponservice.cc，均有功能级 @brief 注释）----
    void onPing(const TcpConnctionptr& conn, const json& js);
    void onIssue(const TcpConnctionptr& conn, const json& js);
    void onClaim(const TcpConnctionptr& conn, const json& js);
    void onUse(const TcpConnctionptr& conn, const json& js);
    void onQueryMine(const TcpConnctionptr& conn, const json& js);
    void onQuery(const TcpConnctionptr& conn, const json& js);

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
    static constexpr int kCleanIntervalSec = 60;
};
