#pragma once
#include <mysql/mysql.h>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

/** @brief 查询结果集：mysql_store_result 一次性取全部数据，行列/列名访问 */
class ResultSet
{
public:
    // 构造空结果集
    ResultSet();
    // 析构
    ~ResultSet();

    size_t rows() const { return data_.size(); }
    size_t cols() const { return colNames_.size(); }
    bool empty() const { return data_.empty(); }

    // 行内取值
    std::string get(size_t row, size_t col) const;
    // 按行+列名取值
    std::string get(size_t row, const std::string& colName) const;

    uint64_t affectedRows() const { return affectedRows_; }
    void setAffectedRows(uint64_t n) { affectedRows_ = n; }
    uint64_t lastInsertId() const { return lastInsertId_; }
    void setLastInsertId(uint64_t id) { lastInsertId_ = id; }

private:
    friend class MysqlConn;
    std::vector<std::string> colNames_;
    std::vector<std::vector<std::string>> data_;
    uint64_t affectedRows_ = 0;
    uint64_t lastInsertId_ = 0;

    // 填充结果集
    void fill(MYSQL_RES* res);
};

/** @brief MySQL 连接池：空闲队列 + 条件变量阻塞等待，借还 RAII 闭环 */
class MysqlPool
{
public:
    MysqlPool(const std::string& host, int port,
              const std::string& user, const std::string& passwd,
              const std::string& db, int poolSize = 8);
    // 析构：关闭全部连接
    ~MysqlPool();

    MysqlPool(const MysqlPool&) = delete;
    MysqlPool& operator=(const MysqlPool&) = delete;

    // 预创建 poolSize 个连接；失败返回 false（并记录具体失败个数）
    bool init();

    // 连接失败自动重连一次；重连成功后更新调用方持有的 conn 指针
    bool ensureAlive(MYSQL*& conn);

private:
    friend class MysqlConn;
    // 功能：取连接（空闲队列阻塞等待）
    MYSQL* doGet();
    // 功能：归还连接入队
    void doPut(MYSQL* conn);

    static constexpr int kGetTimeoutMs = 3000;

    std::string host_;
    int port_;
    std::string user_;
    std::string passwd_;
    std::string db_;
    int poolSize_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::vector<MYSQL*> idle_;
};

/** @brief RAII 连接守卫：构造借出连接，析构自动归还（异常路径也安全） */
class MysqlConn
{
public:
    // RAII 构造：借出连接
    explicit MysqlConn(MysqlPool* pool);
    // RAII 析构：归还连接
    ~MysqlConn();

    MysqlConn(const MysqlConn&) = delete;
    MysqlConn& operator=(const MysqlConn&) = delete;

    // 执行写操作（INSERT/UPDATE/DELETE），结果写入 rs
    bool execute(const std::string& sql, ResultSet* rs);
    // 执行查询（SELECT），结果写入 rs
    bool query(const std::string& sql, ResultSet* rs);

    MYSQL* raw() { return conn_; }

    int lastErrno() const { return lastErrno_; }

private:
    MysqlPool* pool_;
    MYSQL* conn_;
    int lastErrno_ = 0;
};

static constexpr int kShardCount = 5;

inline std::string userCouponTable(int64_t userId)
{
    return "user_coupon_" + std::to_string(static_cast<int>(userId % kShardCount));
}
