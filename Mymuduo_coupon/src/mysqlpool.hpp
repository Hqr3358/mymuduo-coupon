#pragma once
#include <mysql/mysql.h>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

 */
class ResultSet
{
public:
    ResultSet();
    ~ResultSet();

    size_t rows() const { return data_.size(); }
    size_t cols() const { return colNames_.size(); }
    bool empty() const { return data_.empty(); }

    // 行内取值
    std::string get(size_t row, size_t col) const;
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

    void fill(MYSQL_RES* res);
};

/**
 * @brief MySQL 连接池
 *   业务线程池里每个请求都要访问 MySQL，为每个请求新建 TCP 连接代价高，
 *   故预创建 poolSize 个连接，get/release 用互斥锁保护空闲队列。
 *   MysqlConn 是 RAII 守卫：构造取连接，析构归还。
 *
 * 优化：取连接支持有限等待（doGet 在空闲队列为空时用条件变量等待
 * kGetTimeoutMs，超时仍无连接才返回 nullptr），避免高并发瞬时耗尽
 * 连接池时业务请求直接失败。
 */
class MysqlPool
{
public:
    MysqlPool(const std::string& host, int port,
              const std::string& user, const std::string& passwd,
              const std::string& db, int poolSize = 8);
    ~MysqlPool();

    MysqlPool(const MysqlPool&) = delete;
    MysqlPool& operator=(const MysqlPool&) = delete;

    // 预创建 poolSize 个连接；失败返回 false（并记录具体失败个数）
    bool init();

    // 连接失败自动重连一次；重连成功后更新调用方持有的 conn 指针
    bool ensureAlive(MYSQL*& conn);

private:
    friend class MysqlConn;
    MYSQL* doGet();
    void doPut(MYSQL* conn);

    /** 优化：取连接最长等待时间（毫秒），超时返回 nullptr */
    static constexpr int kGetTimeoutMs = 3000;

    std::string host_;
    int port_;
    std::string user_;
    std::string passwd_;
    std::string db_;
    int poolSize_;
    std::mutex mtx_;
    /** 优化：空闲连接就绪条件变量（doPut 归还时唤醒等待者） */
    std::condition_variable cv_;
    std::vector<MYSQL*> idle_;
};

// RAII 连接守卫：作用域内持有一个可用连接，作用域结束自动归还
class MysqlConn
{
public:
    explicit MysqlConn(MysqlPool* pool);
    ~MysqlConn();

    MysqlConn(const MysqlConn&) = delete;
    MysqlConn& operator=(const MysqlConn&) = delete;

    // 执行写操作（INSERT/UPDATE/DELETE），结果写入 rs
    bool execute(const std::string& sql, ResultSet* rs);
    // 执行查询（SELECT），结果写入 rs
    bool query(const std::string& sql, ResultSet* rs);

    MYSQL* raw() { return conn_; }

    /**
     * @brief 优化：最近一次 execute/query 失败时 MySQL 的错误号
     *   （如 ER_DUP_ENTRY=1062 唯一索引冲突），供业务区分失败原因
     */
    int lastErrno() const { return lastErrno_; }

private:
    MysqlPool* pool_;
    MYSQL* conn_;
    /** 优化：最近一次执行失败的错误号（无失败时为 0） */
    int lastErrno_ = 0;
};

 */
static constexpr int kShardCount = 5;

 */
inline std::string userCouponTable(int64_t userId)
{
    return "user_coupon_" + std::to_string(static_cast<int>(userId % kShardCount));
}
