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

    // 单例入口
    static CouponService* init();

    // 按 msgid 分发请求（协议解析在 CouponServer 完成）
    void dispatch(const TcpConnctionptr& conn, const json& js);

    bool ready() const { return ready_; }
    const std::string& error() const { return error_; }

    // 功能：启动过期券定时清理线程
    void startCleaner();

private:
    // 构造：初始化 MySQL 连接池 + Redis
    CouponService();

    // ---- 业务处理（实现见 couponservice.cc，均有功能级 @brief 注释）----
    void onPing(const TcpConnctionptr& conn, const json& js);
    // 功能：发券（建券模板 + 初始化 Redis 库存）
    void onIssue(const TcpConnctionptr& conn, const json& js);
    // 功能：领券（Lua 原子防超发 + 唯一索引兜底）
    void onClaim(const TcpConnctionptr& conn, const json& js);
    // 功能：核销（版本号乐观锁 + 幂等）
    void onUse(const TcpConnctionptr& conn, const json& js);
    // 功能：查询我的券（user_id 分表路由）
    void onQueryMine(const TcpConnctionptr& conn, const json& js);
    // 功能：查询券模板详情
    void onQuery(const TcpConnctionptr& conn, const json& js);

    // 功能：单轮清理（过期券下架 + Redis 清缓存）
    void cleanExpired();

    // ---- 工具 ----
        // 统一响应回包
void sendResp(const TcpConnctionptr& conn, int msgid, int err,
                  const std::string& msg, const json& data = json::object());
    // 生成全局唯一券码
    static std::string genCode(int64_t couponId, int64_t userId);
    // 当前时间字符串
    static std::string nowStr();
    // 未来时间字符串
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
