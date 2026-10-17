#include "mysqlpool.hpp"
#include <chrono>
#include <cstring>

// ==================== ResultSet ====================

/**
 * @brief 结果集：一次性取回全部数据（mysql_store_result），提供行列/列名访问
 */
ResultSet::ResultSet() {}
ResultSet::~ResultSet() {}

/**
 * @brief 填充结果集：解析 mysql_store_result 的行列数据，并释放 MYSQL_RES
 */
void ResultSet::fill(MYSQL_RES* res)
{
    if (!res) return;

    unsigned int ncols = mysql_num_fields(res);
    MYSQL_FIELD* fields = mysql_fetch_fields(res);
    colNames_.reserve(ncols);
    for (unsigned int i = 0; i < ncols; ++i)
    {
        colNames_.emplace_back(fields[i].name);
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr)
    {
        unsigned long* lengths = mysql_fetch_lengths(res);
        std::vector<std::string> r;
        r.reserve(ncols);
        for (unsigned int i = 0; i < ncols; ++i)
        {
            r.emplace_back(row[i] ? std::string(row[i], lengths[i]) : std::string());
        }
        data_.emplace_back(std::move(r));
    }
    mysql_free_result(res);
}

/**
 * @brief 按 (行, 列) 索引取值，越界返回空串
 */
std::string ResultSet::get(size_t row, size_t col) const
{
    if (row >= data_.size() || col >= data_[row].size()) return {};
    return data_[row][col];
}

/**
 * @brief 按 (行, 列名) 取值，列名不存在返回空串
 */
std::string ResultSet::get(size_t row, const std::string& colName) const
{
    if (row >= data_.size()) return {};
    for (size_t c = 0; c < colNames_.size(); ++c)
    {
        if (colNames_[c] == colName) return data_[row][c];
    }
    return {};
}

// ==================== MysqlPool ====================

/**
 * @brief 构造连接池：仅保存连接参数，实际连接在 init() 建立
 */
MysqlPool::MysqlPool(const std::string& host, int port,
                     const std::string& user, const std::string& passwd,
                     const std::string& db, int poolSize)
    : host_(host), port_(port), user_(user), passwd_(passwd), db_(db), poolSize_(poolSize)
{
}

/**
 * @brief 析构：关闭连接池全部空闲连接
 */
MysqlPool::~MysqlPool()
{
    for (MYSQL* c : idle_)
    {
        mysql_close(c);
    }
    idle_.clear();
}

/**
 * @brief 初始化连接池：建立 poolSize 条 MySQL 连接入空闲队列
 * @details 对应功能测试第 11 项连接池等待：预建连接避免并发时反复建连。
 */
bool MysqlPool::init()
{
    int ok = 0;
    for (int i = 0; i < poolSize_; ++i)
    {
        MYSQL* c = mysql_init(nullptr);
        if (!c) continue;
        mysql_options(c, MYSQL_SET_CHARSET_NAME, "utf8mb4");
        if (!mysql_real_connect(c, host_.c_str(), user_.c_str(), passwd_.c_str(),
                                db_.c_str(), port_, nullptr, 0))
        {
            mysql_close(c);
            continue;
        }
        idle_.push_back(c);
        ++ok;
    }
    return ok > 0;
}

/**
 * @brief 连接保活：ping 探测，失效则重连一次并写回新连接指针
 */
bool MysqlPool::ensureAlive(MYSQL*& conn)
{
    if (!conn) return false;
    // ping 探测；失效则重连一次，并把新连接写回调用方指针
    if (mysql_ping(conn) == 0) return true;
    mysql_close(conn);
    MYSQL* fresh = mysql_init(nullptr);
    if (!fresh) return false;
    mysql_options(fresh, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    if (!mysql_real_connect(fresh, host_.c_str(), user_.c_str(), passwd_.c_str(),
                            db_.c_str(), port_, nullptr, 0))
    {
        mysql_close(fresh);
        return false;
    }
    conn = fresh;
    return true;
}

/**
 * @brief 【功能】连接池取连接：空闲队列阻塞等待（条件变量机制）
 * @details 对应功能测试第 11 项「连接池等待」：空闲队列为空时
 *          cv_.wait_for 有限等待 kGetTimeoutMs(3s)，期间有连接归还
 *          （doPut 中 notify_one）即唤醒；超时仍无连接才返回 nullptr，
 *          由业务层走 ERR_DB/重试路径，避免高并发瞬时耗尽连接池时直接失败。
 * @return 空闲 MYSQL* 连接；超时无连接返回 nullptr
 */
MYSQL* MysqlPool::doGet()
{
    // 优化：空闲队列为空时条件变量有限等待（3s），期间归还的连接会唤醒
    // 本线程；超时仍无连接才返回 nullptr，避免高并发下瞬时失败。
    std::unique_lock<std::mutex> lock(mtx_);
    if (!cv_.wait_for(lock, std::chrono::milliseconds(kGetTimeoutMs),
                      [this] { return !idle_.empty(); }))
    {
        return nullptr;
    }
    MYSQL* c = idle_.back();
    idle_.pop_back();
    return c;
}

/**
 * @brief 【功能】连接池归还连接：入队并唤醒一个等待线程
 * @details RAII（MysqlConn 析构）调用；notify_one 唤醒 doGet 中
 *          条件变量等待的线程，保证高并发下连接能被及时复用。
 * @param conn 待归还的 MYSQL* 连接
 */
void MysqlPool::doPut(MYSQL* conn)
{
    if (!conn) return;
    std::lock_guard<std::mutex> lock(mtx_);
    idle_.push_back(conn);
    cv_.notify_one(); // 优化：唤醒一个正在等待连接的线程
}

// ==================== MysqlConn（RAII）====================

/**
 * @brief RAII 连接守卫：构造时从连接池借出连接，析构自动归还
 * @details 对应功能测试第 11 项：借出时 doGet 阻塞等待空闲连接，
 *          异常路径下析构仍归还，保证连接不泄漏。
 */
MysqlConn::MysqlConn(MysqlPool* pool)
    : pool_(pool), conn_(pool ? pool->doGet() : nullptr)
{
}

/** @brief 析构：归还连接池（RAII 借还闭环，异常路径也归还） */
MysqlConn::~MysqlConn()
{
    if (pool_ && conn_)
    {
        pool_->doPut(conn_);
    }
}

/**
 * @brief 执行写/改 SQL（execute/update/insert 等），可选带回结果集
 * @details 对应功能测试第 12 项 1062 兜底：INSERT 触发唯一索引冲突时
 *          MySQL 返回 1062 错误码，由业务层捕获转 ERR_DUP_CLAIM。
 */
bool MysqlConn::execute(const std::string& sql, ResultSet* rs)
{
    if (!conn_) return false;
    if (!pool_->ensureAlive(conn_)) return false;

    if (mysql_real_query(conn_, sql.data(), sql.size()) != 0)
    {
        lastErrno_ = mysql_errno(conn_); // 优化：记录错误号（如 1062 唯一索引冲突）
        return false;
    }
    lastErrno_ = 0; // 优化：执行成功清空错误号
    if (rs)
    {
        rs->setAffectedRows(mysql_affected_rows(conn_));
        rs->setLastInsertId(mysql_insert_id(conn_));
        // UPDATE/INSERT 无结果集，但保险起见消费掉
        MYSQL_RES* res = mysql_store_result(conn_);
        if (res)
        {
            rs->fill(res);
        }
    }
    return true;
}

/**
 * @brief 执行查询 SQL（SELECT），结果填充 ResultSet
 */
bool MysqlConn::query(const std::string& sql, ResultSet* rs)
{
    if (!conn_ || !rs) return false;
    if (!pool_->ensureAlive(conn_)) return false;

    if (mysql_real_query(conn_, sql.data(), sql.size()) != 0)
    {
        lastErrno_ = mysql_errno(conn_); // 优化：记录错误号
        return false;
    }
    lastErrno_ = 0; // 优化：执行成功清空错误号
    MYSQL_RES* res = mysql_store_result(conn_);
    if (!res)
    {
        return false;
    }
    rs->fill(res);
    return true;
}
