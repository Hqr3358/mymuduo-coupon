# CLAUDE.md

本文件给 Claude Code（以及任何 AI 编码助手）读，帮助理解这个项目的架构约定、构建方式和编码规范。

## 项目一句话

基于自研 Mymuduo 网络库（one loop per thread 反应器）的高并发优惠券系统，覆盖**发券 / 领券 / 核销 / 查询**四条主链路，Redis Lua 原子防超发 + MySQL 分表唯一索引兜底。

## 目录结构

```
mymuduo-coupon/
├── Mymuduo/                # 自研 muduo 网络库（EventLoop/Channel/Buffer/TcpServer/业务线程池）
├── Mymuduo_coupon/         # 优惠券业务
│   ├── src/                # 协议、解码器、MySQL 连接池、Redis 客户端、业务服务
│   ├── client/             # 压测/演示客户端（纯 POSIX socket）
│   ├── sql/init.sql        # 建库 + 券模板表 + 5 张 user_coupon 分表
│   └── redis/claim_coupon.lua
└── build/                  # 构建产物（gitignore）
```

## 构建与测试

```bash
# 一次性构建（网络库 + 业务）
cd build && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .. && make -j4

# 产物
#   build/Mymuduo_coupon/couponserver    服务端
#   build/Mymuduo_coupon/couponclient    压测客户端

# 初始化数据库
mysql -uroot < Mymuduo_coupon/sql/init.sql

# 启动服务端（参数：port businessThreads，默认 9081 8）
LD_LIBRARY_PATH=Mymuduo/lib ./build/Mymuduo_coupon/couponserver 9081 8

# 快速验证
./build/Mymuduo_coupon/couponclient ping
./build/Mymuduo_coupon/couponclient issue "秒杀券" 100
./build/Mymuduo_coupon/couponclient stress <coupon_id> 300 16
./build/Mymuduo_coupon/couponclient metrics
```

一键脚本：`scripts/build_and_test.sh`（cmake + make + ping 检查）。

## 架构硬约定（改代码时不要破坏）

1. **线程模型**：baseloop + N 个 sub-reactor 跑 IO；**业务回调在 BusinessThreadPool（8 线程）执行**，不在 IO 线程里做 MySQL/Redis 阻塞调用。
2. **Redis 连接**：hiredis context 非线程安全，业务线程用 `thread_local` 懒连接，**禁止跨线程共用同一 redisContext**。
3. **MySQL 连接**：用 `MysqlConn` RAII 从 `MysqlPool` 取连接，构造借出、析构归还，**不要手动 new/delete MYSQL***。
4. **跨线程 send**：必须走 `TcpConnection::send`（内部 queueInLoop 到 IO 线程），**不能在业务线程直接 write fd**。
5. **分表路由**：`user_coupon_{user_id % 5}`，同一用户固定一张表，**不要跨表 JOIN 或分布式事务**。
6. **领券一致性**：Lua 在 Redis 里原子 DECR+SADD；DB INSERT 失败时按 errno 区分——1062 唯一索引冲突只回补库存不 SREM，其他错误完整回滚。
7. **核销幂等**：`UPDATE ... WHERE status=0 AND version=?`，affectedRows==0 就重试（最多 3 次），不要用 SELECT 后再 UPDATE 的非原子写法。

## 编码规范

- 头文件一律 `#pragma once`，不用 `#ifndef/#define/#endif` 守卫。
- 不允许裸 `new/delete`；网络库用 `shared_ptr` 管理连接生命周期，Buffer 用值语义。
- 日志用项目自带的 `Logmanage(LEVEL, fmt, ...)`，不要 printf。
- 错误码在 `src/protocol.hpp` 的 `ErrCode` 里，新增错误码集中定义，不要在业务代码里写魔术数字。
- 新增 msgid 时：① protocol.hpp 加枚举；② CouponService 构造函数注册 handler；③ client.cc 同步加枚举和命令分支。

## 改代码节奏（老项目改造）

- 先读相关文件再说，不要凭命名猜实现。
- 一次只改一个方向，改完跑 `scripts/build_and_test.sh` 和一条压测命令回归。
- 不要顺手"重构"跟当前任务无关的代码——这个项目能跑，破坏了不好排查。
- 涉及 Redis key、SQL、错误码的改动，同步更新本文件和 `sql/init.sql`。
