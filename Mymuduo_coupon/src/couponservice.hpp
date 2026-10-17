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
     */
    void onPing(const TcpConnctionptr& conn, const json& js);
     */
    void onIssue(const TcpConnctionptr& conn, const json& js);
     */
    void onClaim(const TcpConnctionptr& conn, const json& js);
     */
    void onUse(const TcpConnctionptr& conn, const json& js);
     */
    void onQueryMine(const TcpConnctionptr& conn, const json& js);
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
