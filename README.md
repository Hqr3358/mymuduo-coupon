# mymuduo-coupon

基于自研 Mymuduo 网络库（one loop per thread 反应器模型）的高并发优惠券系统，覆盖**发券 / 领券 / 核销 / 查询**四条主链路，Redis Lua 原子防超发 + MySQL 分表唯一索引兜底。

## 技术栈

- **网络层**：自研 Mymuduo（EventLoop / Channel / Buffer / TcpServer / BusinessThreadPool），one loop per thread + 业务线程池
- **存储**：MySQL（分表）、Redis（hiredis，thread_local 连接）
- **并发**：Lua 原子脚本、版本号乐观锁、原子计数器、CAS
- **语言**：C++17，JSON 用 nlohmann/json

## 核心链路

| 接口 | 功能 | 关键设计 |
|---|---|---|
| issue | 发券 | DB 落库 + Redis 库存初始化 + 元数据缓存 |
| claim | 领券 | Cache-Aside 读元数据 → Lua 原子扣库存+SADD 去重 → DB INSERT，1062 冲突只回补库存 |
| use | 核销 | 版本号乐观锁 `UPDATE ... WHERE status=0 AND version=?`，冲突重试 3 次 |
| mine | 我的券 | 联合游标分页（`WHERE id < max_id`），每页 20 条 |
| metrics | 运行计数 | 按 errno 原子统计，压测时用 |

## 防超发 / 防重复 设计

1. **Redis Lua**：`DECR stock` + `SADD claimed:{uid}` 原子执行，库存为 0 或已领过直接返回
2. **DB 唯一索引**：`uk(user_id, coupon_id)` 兜底，INSERT 冲突时按 errno 区分：
   - 1062（重复）→ 只回补库存，不 SREM（避免误删别人的去重标记）
   - 其他错误 → 完整回滚库存 + SREM
3. **核销幂等**：`version` 字段乐观锁，affectedRows==0 就重读重试，不用 `SELECT ... FOR UPDATE`

## 缓存设计

- **券元数据 Cache-Aside**：领券先读 Redis `coupon:info:{id}`，miss 回查 DB 并回填
- **空值缓存**：券不存在时写 `coupon:missing:{id}`（TTL 60s）防穿透
- **TTL 随机偏移**：库存/元数据 TTL 加 0~300s 随机量，防一批券同时过期雪崩
- **UNLINK 异步删**：清理大 Set（claimed）时用 UNLINK 不阻塞 Redis 单线程
- **核销缓存**：`coupon:used:{code}` 7 天 TTL，加速幂等判定

## 压测客户端

```bash
# 发券
./couponclient issue "秒杀券" 1000

# 3000 用户并发领券，16 线程
./couponclient stress 1 3000 16
# 输出：success / no_stock / duplicate / QPS / P50 / P95 / P99

# 同一用户重复领（验证幂等）
./couponclient dup 1 1001 10

# 查看服务端运行计数
./couponclient metrics
```

## 目录结构

```
├── Mymuduo/                # 自研 muduo 网络库
├── Mymuduo_coupon/
│   ├── src/                # 协议、解码器、MySQL 连接池、Redis 客户端、业务服务
│   ├── client/client.cc    # 压测客户端（纯 POSIX socket）
│   ├── sql/init.sql        # 建库 + 5 张 user_coupon 分表
│   └── redis/claim_coupon.lua
├── scripts/build_and_test.sh
└── CLAUDE.md               # 给 AI 编码助手读的架构约定
```

## 构建

```bash
# 依赖：libhiredis-dev、nlohmann-json3-dev、libmysqlclient-dev
bash scripts/build_and_test.sh

# 产物
#   build/Mymuduo_coupon/couponserver
#   build/Mymuduo_coupon/couponclient

# 初始化数据库
mysql -uroot < Mymuduo_coupon/sql/init.sql

# 启动服务端（port businessThreads，默认 9081 8）
LD_LIBRARY_PATH=Mymuduo/lib ./build/Mymuduo_coupon/couponserver
```

## 线程模型

- baseloop + N 个 sub-reactor 跑 IO（读包、解协议、发响应）
- BusinessThreadPool（8 线程）跑业务回调（MySQL/Redis 阻塞调用不占 IO 线程）
- Redis 连接按 thread_local 懒创建（hiredis context 非线程安全）
- MySQL 连接从 MysqlPool RAII 借出/归还
