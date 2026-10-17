#include "couponservice.hpp"
#include "log.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <random>
#include <thread>

// ==================== 单例 ====================

CouponService* CouponService::init()
{
    static CouponService svc;
    return &svc;
}

CouponService::CouponService()
    : mysql_("127.0.0.1", 3306, "coupon", "coupon123", "coupon_db", 8)
{
    ready_ = mysql_.init();
    if (!ready_)
    {
        error_ = "mysql pool init failed";
        Logmanage(ERR, "%s", error_.c_str());
    }
    if (ready_ && !redis_.init("127.0.0.1", 6379))
    {
        ready_ = false;
        error_ = "redis init failed";
        Logmanage(ERR, "%s", error_.c_str());
    }

    // ---- 注册服务（msgid -> handler）----
    services_.insert({MSG_PING, std::bind(&CouponService::onPing, this, std::placeholders::_1, std::placeholders::_2)});
    services_.insert({MSG_ISSUE, std::bind(&CouponService::onIssue, this, std::placeholders::_1, std::placeholders::_2)});
    services_.insert({MSG_CLAIM, std::bind(&CouponService::onClaim, this, std::placeholders::_1, std::placeholders::_2)});
    services_.insert({MSG_USE, std::bind(&CouponService::onUse, this, std::placeholders::_1, std::placeholders::_2)});
    services_.insert({MSG_QUERY_MINE, std::bind(&CouponService::onQueryMine, this, std::placeholders::_1, std::placeholders::_2)});
    services_.insert({MSG_QUERY, std::bind(&CouponService::onQuery, this, std::placeholders::_1, std::placeholders::_2)});

    if (ready_)
    {
        Logmanage(NORMOL, "CouponService ready");
    }
}

// ==================== 分发 ====================

void CouponService::dispatch(const TcpConnctionptr& conn, const json& js)
{
    if (!js.is_object() || !js.contains("msgid") || !js["msgid"].is_number_integer())
    {
        sendResp(conn, MSG_ERROR, ERR_ARGS, "missing msgid");
        return;
    }

    int msgid = js["msgid"].get<int>();
    auto it = services_.find(msgid);
    if (it == services_.end())
    {
        sendResp(conn, MSG_ERROR, ERR_ARGS, "unknown msgid: " + std::to_string(msgid));
        return;
    }
    it->second(conn, js);
}

// ==================== 业务处理 ====================

void CouponService::onPing(const TcpConnctionptr& conn, const json& /*js*/)
{
    sendResp(conn, MSG_PING, ERR_OK, "pong", {{"pong", true}});
}

// ---- 发券：创建券模板 + 初始化 Redis 库存 ----
void CouponService::onIssue(const TcpConnctionptr& conn, const json& js)
{
    if (!js.contains("title") || !js["title"].is_string() ||
        !js.contains("total") || !js["total"].is_number_integer())
    {
        sendResp(conn, MSG_ISSUE, ERR_ARGS, "need title & total");
        return;
    }

    std::string title = js["title"].get<std::string>();
    int total = js["total"].get<int>();
    if (title.empty() || total <= 0)
    {
        sendResp(conn, MSG_ISSUE, ERR_ARGS, "invalid title or total");
        return;
    }

    std::string start = js.contains("start_time") ? js["start_time"].get<std::string>() : nowStr();
    std::string end = js.contains("end_time") ? js["end_time"].get<std::string>() : futureStr(30);

    std::string sql = "INSERT INTO coupon(title, total, start_time, end_time, status) "
                      "VALUES('" + title + "', " + std::to_string(total) +
                      ", '" + start + "', '" + end + "', 1)";

    MysqlConn mc(&mysql_);
    ResultSet rs;
    if (!mc.execute(sql, &rs))
    {
        sendResp(conn, MSG_ISSUE, ERR_DB, "insert coupon failed");
        return;
    }

    int64_t couponId = static_cast<int64_t>(rs.lastInsertId());
    redis_.initStock(couponId, total);

    sendResp(conn, MSG_ISSUE, ERR_OK, "issue ok",
             {{"coupon_id", couponId}, {"total", total}});
}

// ---- 领券：Redis Lua 原子防超发 + MySQL 唯一索引兜底 ----
void CouponService::onClaim(const TcpConnctionptr& conn, const json& js)
{
    if (!js.contains("coupon_id") || !js.contains("user_id") ||
        !js["coupon_id"].is_number_integer() || !js["user_id"].is_number_integer())
    {
        sendResp(conn, MSG_CLAIM, ERR_ARGS, "need coupon_id & user_id");
        return;
    }

    int64_t couponId = js["coupon_id"].get<int64_t>();
    int64_t userId = js["user_id"].get<int64_t>();

    // 1) 校验券存在且未过期（读 MySQL）
    std::string checkSql = "SELECT id, title, status, end_time FROM coupon WHERE id = " + std::to_string(couponId);
    ResultSet check;
    {
        MysqlConn mc(&mysql_);
        if (!mc.query(checkSql, &check))
        {
            sendResp(conn, MSG_CLAIM, ERR_DB, "query coupon failed");
            return;
        }
    }
    if (check.empty())
    {
        sendResp(conn, MSG_CLAIM, ERR_NOT_EXIST, "coupon not exist");
        return;
    }
    if (check.get(0, "status") != "1")
    {
        sendResp(conn, MSG_CLAIM, ERR_NOT_EXIST, "coupon offline");
        return;
    }
    if (check.get(0, "end_time") < nowStr())
    {
        sendResp(conn, MSG_CLAIM, ERR_NOT_EXIST, "coupon expired");
        return;
    }
    std::string title = check.get(0, "title");

    // 2) Redis Lua 原子扣库存 + 去重
    long remain = 0;
    int ret = redis_.claimTry(couponId, userId, &remain);
    if (ret == ERR_NO_STOCK)
    {
        sendResp(conn, MSG_CLAIM, ERR_NO_STOCK, "no stock");
        return;
    }
    if (ret == ERR_DUP_CLAIM)
    {
        sendResp(conn, MSG_CLAIM, ERR_DUP_CLAIM, "already claimed");
        return;
    }
    if (ret == ERR_NOT_EXIST)
    {
        sendResp(conn, MSG_CLAIM, ERR_NOT_EXIST, "coupon stock missing, re-issue needed");
        return;
    }
    if (ret != ERR_OK)
    {
        sendResp(conn, MSG_CLAIM, ERR_SYSTEM, "redis unavailable, retry later");
        return;
    }

    // 3) 生成券码并落库（唯一索引兜底防重复；失败则回滚 Redis 预扣）
    std::string code = genCode(couponId, userId);
    std::string shard = userCouponTable(userId);
    std::string insertSql = "INSERT INTO " + shard +
                            "(user_id, coupon_id, coupon_code, status, claimed_at) "
                            "VALUES(" + std::to_string(userId) + ", " + std::to_string(couponId) +
                            ", '" + code + "', 0, '" + nowStr() + "')";

    MysqlConn mc(&mysql_);
    ResultSet rs;
    if (!mc.execute(insertSql, &rs))
    {
        // 优化：区分"唯一索引冲突(ER_DUP_ENTRY=1062)"与一般 DB 错误：
        //   - 1062：DB 已存在该用户领取记录（如 Redis 数据丢失后状态修复），
        //     说明用户实际已领到过该券 → 保持 claimed 集合（用户已在其中，
        //     后续请求在 Redis 层直接返回 duplicate），仅回补本次预扣的库存，
        //     按"重复领取"幂等返回，避免 SREM 移除集合后反复打 DB；
        //   - 其他错误：完整回滚 Redis 预扣（库存 + claimed 集合）。
        if (mc.lastErrno() == 1062)
        {
            redis_.claimRollbackStock(couponId);
            sendResp(conn, MSG_CLAIM, ERR_DUP_CLAIM, "already claimed (db unique index)");
            return;
        }
        // 唯一索引冲突（用户已领过）或其他 DB 错误：回滚 Redis 预扣
        redis_.claimRollback(couponId, userId);
        sendResp(conn, MSG_CLAIM, ERR_DB, "insert user_coupon failed, rolled back");
        return;
    }

    sendResp(conn, MSG_CLAIM, ERR_OK, "claim ok",
             {{"coupon_id", couponId}, {"user_id", userId},
              {"coupon_code", code}, {"remain", remain}, {"title", title}});
}

// ---- 核销：MySQL 原子 UPDATE（幂等）+ Redis 快速缓存 ----
void CouponService::onUse(const TcpConnctionptr& conn, const json& js)
{
    if (!js.contains("user_id") || !js.contains("coupon_code") ||
        !js["user_id"].is_number_integer() || !js["coupon_code"].is_string())
    {
        sendResp(conn, MSG_USE, ERR_ARGS, "need user_id & coupon_code");
        return;
    }

    int64_t userId = js["user_id"].get<int64_t>();
    std::string code = js["coupon_code"].get<std::string>();

    // 快速幂等路径：Redis 缓存命中"已核销"
    if (redis_.isUsed(code))
    {
        sendResp(conn, MSG_USE, ERR_ALREADY_USED, "already used (cache)");
        return;
    }

    std::string orderId = "USE" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    std::string shard = userCouponTable(userId);
    std::string updateSql = "UPDATE " + shard +
                            " SET status = 1, used_at = '" + nowStr() +
                            "', order_id = '" + orderId +
                            "' WHERE coupon_code = '" + code +
                            "' AND user_id = " + std::to_string(userId) + " AND status = 0";

    MysqlConn mc(&mysql_);
    ResultSet rs;
    if (!mc.execute(updateSql, &rs))
    {
        sendResp(conn, MSG_USE, ERR_DB, "use coupon failed");
        return;
    }

    if (rs.affectedRows() == 1)
    {
        redis_.markUsed(code); // 核销成功，写缓存加速后续幂等判定
        sendResp(conn, MSG_USE, ERR_OK, "use ok", {{"order_id", orderId}});
        return;
    }

    // 未更新到行：要么已核销（幂等），要么券码不属于该用户
    std::string querySql = "SELECT status FROM " + shard +
                           " WHERE coupon_code = '" + code +
                           "' AND user_id = " + std::to_string(userId);
    ResultSet qr;
    if (!mc.query(querySql, &qr))
    {
        sendResp(conn, MSG_USE, ERR_DB, "query coupon status failed");
        return;
    }
    if (qr.empty())
    {
        sendResp(conn, MSG_USE, ERR_INVALID, "invalid coupon code");
        return;
    }
    if (qr.get(0, "status") == "1")
    {
        sendResp(conn, MSG_USE, ERR_ALREADY_USED, "already used");
        return;
    }
    sendResp(conn, MSG_USE, ERR_SYSTEM, "use conflict, retry later");
}

// ---- 查询我的券（分表查询，天然只查一张表）----
void CouponService::onQueryMine(const TcpConnctionptr& conn, const json& js)
{
    if (!js.contains("user_id") || !js["user_id"].is_number_integer())
    {
        sendResp(conn, MSG_QUERY_MINE, ERR_ARGS, "need user_id");
        return;
    }

    int64_t userId = js["user_id"].get<int64_t>();
    std::string shard = userCouponTable(userId);

    std::string sql = "SELECT uc.coupon_code, uc.status, uc.claimed_at, uc.used_at, uc.order_id, "
                      "c.title, c.id AS coupon_id "
                      "FROM " + shard + " uc JOIN coupon c ON uc.coupon_id = c.id "
                      "WHERE uc.user_id = " + std::to_string(userId) +
                      " ORDER BY uc.id DESC LIMIT 100";

    MysqlConn mc(&mysql_);
    ResultSet rs;
    if (!mc.query(sql, &rs))
    {
        sendResp(conn, MSG_QUERY_MINE, ERR_DB, "query failed");
        return;
    }

    json list = json::array();
    for (size_t i = 0; i < rs.rows(); ++i)
    {
        list.push_back({
            {"coupon_id", rs.get(i, "coupon_id")},
            {"title", rs.get(i, "title")},
            {"coupon_code", rs.get(i, "coupon_code")},
            {"status", rs.get(i, "status")},           // 0 未用 1 已核销
            {"claimed_at", rs.get(i, "claimed_at")},
            {"used_at", rs.get(i, "used_at")},
            {"order_id", rs.get(i, "order_id")},
        });
    }
    sendResp(conn, MSG_QUERY_MINE, ERR_OK, "ok", {{"count", list.size()}, {"coupons", list}});
}

// ---- 查询券模板（含 Redis 实时库存）----
void CouponService::onQuery(const TcpConnctionptr& conn, const json& js)
{
    if (!js.contains("coupon_id") || !js["coupon_id"].is_number_integer())
    {
        sendResp(conn, MSG_QUERY, ERR_ARGS, "need coupon_id");
        return;
    }

    int64_t couponId = js["coupon_id"].get<int64_t>();
    std::string sql = "SELECT id, title, total, start_time, end_time, status "
                      "FROM coupon WHERE id = " + std::to_string(couponId);

    MysqlConn mc(&mysql_);
    ResultSet rs;
    if (!mc.query(sql, &rs))
    {
        sendResp(conn, MSG_QUERY, ERR_DB, "query failed");
        return;
    }
    if (rs.empty())
    {
        sendResp(conn, MSG_QUERY, ERR_NOT_EXIST, "coupon not exist");
        return;
    }

    long stock = redis_.getStock(couponId);
    sendResp(conn, MSG_QUERY, ERR_OK, "ok", {
        {"coupon_id", rs.get(0, "id")},
        {"title", rs.get(0, "title")},
        {"total", rs.get(0, "total")},
        {"remain", stock < 0 ? -1 : stock},
        {"start_time", rs.get(0, "start_time")},
        {"end_time", rs.get(0, "end_time")},
        {"status", rs.get(0, "status")},
    });
}

// ==================== 工具 ====================

void CouponService::sendResp(const TcpConnctionptr& conn, int msgid, int err,
                             const std::string& msg, const json& data)
{
    json j;
    j["msgid"] = msgid;
    j["errno"] = err;
    j["msg"] = msg;
    j["data"] = data;

    std::string out;
    encodeFrame(out, j.dump());
    conn->send(out); // send 线程安全：跨线程时自动 queueInLoop 到 IO 线程
}

std::string CouponService::genCode(int64_t couponId, int64_t userId)
{
    static thread_local std::mt19937_64 rng(std::random_device{}());
    char buf[48];
    std::snprintf(buf, sizeof(buf), "C%lld-%lld-%016llx",
                  static_cast<long long>(couponId), static_cast<long long>(userId),
                  static_cast<unsigned long long>(rng()));
    return buf;
}

std::string CouponService::nowStr()
{
    time_t t = time(nullptr);
    struct tm lt;
    localtime_r(&t, &lt);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                  lt.tm_hour, lt.tm_min, lt.tm_sec);
    return buf;
}

std::string CouponService::futureStr(int days)
{
    time_t t = time(nullptr) + static_cast<time_t>(days) * 86400;
    struct tm lt;
    localtime_r(&t, &lt);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                  lt.tm_hour, lt.tm_min, lt.tm_sec);
    return buf;
}

// ==================== 优化：定时清理 ====================

void CouponService::startCleaner()
{
    // 优化：启动后台清理线程，每 kCleanIntervalSec 秒执行一轮 cleanExpired。
    //   线程内首次调用 Redis 时会通过 thread_local 懒加载自己的连接，
    //   与业务线程的连接互不干扰；随进程退出终止，无需显式 join。
    if (cleanerRunning_.exchange(true))
    {
        return; // 已启动，幂等
    }
    cleaner_ = std::thread([this]
                           {
                               while (cleanerRunning_)
                               {
                                   std::this_thread::sleep_for(
                                       std::chrono::seconds(kCleanIntervalSec));
                                   cleanExpired();
                               }
                           });
}

void CouponService::cleanExpired()
{
    // 优化：单轮清理——过期券下架 + Redis 缓存清理 + 分表状态标记
    // 1) 找出"已上线(status=1)且已过期(end_time < NOW())"的券
    ResultSet rs;
    {
        MysqlConn mc(&mysql_);
        if (!mc.query("SELECT id FROM coupon WHERE status = 1 AND end_time < NOW()", &rs))
        {
            Logmanage(ERR, "cleaner: query expired coupons failed");
            return;
        }
    }
    if (rs.empty())
    {
        return;
    }

    int downCount = 0;
    int userMarkCount = 0;
    for (size_t i = 0; i < rs.rows(); ++i)
    {
        int64_t couponId = std::atoll(rs.get(i, "id").c_str());

        // 2) 下架：status 1 -> 0（带 status=1 条件，幂等防重复下架）
        {
            MysqlConn mc(&mysql_);
            ResultSet r1;
            if (!mc.execute("UPDATE coupon SET status = 0 WHERE id = " +
                                std::to_string(couponId) + " AND status = 1", &r1))
            {
                Logmanage(ERR, "cleaner: offline coupon %lld failed", (long long)couponId);
                continue;
            }
            if (r1.affectedRows() == 1)
            {
                ++downCount;
            }
        }

        // 3) 清理 Redis 库存/领取集合缓存（防残留 key 泄漏内存）
        redis_.clearCouponCache(couponId);

        // 4) 将该券在各分表中"未核销(status=0)"的用户券标记为已过期(status=2)
        for (int shard = 0; shard < kShardCount; ++shard)
        {
            std::string tbl = "user_coupon_" + std::to_string(shard);
            MysqlConn mc(&mysql_);
            ResultSet r2;
            std::string sql = "UPDATE " + tbl + " SET status = 2 "
                              "WHERE coupon_id = " + std::to_string(couponId) + " AND status = 0";
            if (mc.execute(sql, &r2))
            {
                userMarkCount += static_cast<int>(r2.affectedRows());
            }
            // 单表失败不中断，继续清理其他分表
        }
    }

    Logmanage(NORMOL, "cleaner: expired coupons offline=%d, user coupons marked expired=%d",
              downCount, userMarkCount);
}
