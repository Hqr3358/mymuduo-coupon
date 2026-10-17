#include "mysqlpool.hpp"
#include <cstring>

// ==================== ResultSet ====================

ResultSet::ResultSet() {}
ResultSet::~ResultSet() {}

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

std::string ResultSet::get(size_t row, size_t col) const
{
    if (row >= data_.size() || col >= data_[row].size()) return {};
    return data_[row][col];
}

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

MysqlPool::MysqlPool(const std::string& host, int port,
                     const std::string& user, const std::string& passwd,
                     const std::string& db, int poolSize)
    : host_(host), port_(port), user_(user), passwd_(passwd), db_(db), poolSize_(poolSize)
{
}

MysqlPool::~MysqlPool()
{
    for (MYSQL* c : idle_)
    {
        mysql_close(c);
    }
    idle_.clear();
}

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

MYSQL* MysqlPool::doGet()
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (idle_.empty()) return nullptr;
    MYSQL* c = idle_.back();
    idle_.pop_back();
    return c;
}

void MysqlPool::doPut(MYSQL* conn)
{
    if (!conn) return;
    std::lock_guard<std::mutex> lock(mtx_);
    idle_.push_back(conn);
}

// ==================== MysqlConn（RAII）====================

MysqlConn::MysqlConn(MysqlPool* pool)
    : pool_(pool), conn_(pool ? pool->doGet() : nullptr)
{
}

MysqlConn::~MysqlConn()
{
    if (pool_ && conn_)
    {
        pool_->doPut(conn_);
    }
}

bool MysqlConn::execute(const std::string& sql, ResultSet* rs)
{
    if (!conn_) return false;
    if (!pool_->ensureAlive(conn_)) return false;

    if (mysql_real_query(conn_, sql.data(), sql.size()) != 0)
    {
        return false;
    }
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

bool MysqlConn::query(const std::string& sql, ResultSet* rs)
{
    if (!conn_ || !rs) return false;
    if (!pool_->ensureAlive(conn_)) return false;

    if (mysql_real_query(conn_, sql.data(), sql.size()) != 0)
    {
        return false;
    }
    MYSQL_RES* res = mysql_store_result(conn_);
    if (!res)
    {
        return false;
    }
    rs->fill(res);
    return true;
}
