# Mymuduo_coupon — 基于自研 muduo 网络库的优惠券系统

一个建立在自研网络库 **Mymuduo**（仿 muduo）之上的完整优惠券业务系统，覆盖
**发券 / 领券 / 核销 / 查询** 四条主链路，核心卖点：

- **Redis Lua 原子防超发**（DECR 库存 + SADD 去重一把锁）
- **MySQL 唯一索引兜底**（防重复领取最终防线）
- **user_coupon 分表**（按 user_id % 16 哈希路由）
- **核销幂等**（MySQL 原子 UPDATE 受影响行数判定）

## 架构总览

```
                         ┌─────────────────────────────────────────────┐
                         │              couponserver                    │
                         │                                              │
   couponclient ────────▶│  自研 Mymuduo 网络库（Tcpserver）              │
  (压测/演示客户端)        │   ├─ EventLoop(baseloop: accept/定时器)      │
   4字节长度头+JSON        │   ├─ sub-reactor ×N（ET 读事件）             │
                         │   ├─ BusinessThreadPool ×8（业务回调线程）    │
                         │   └─ RedisClient(连接活跃 ZSET 统计)          │
                         │                                              │
                         │   CouponServer（应用层）                       │
                         │   ├─ Codec（per-连接累积切包，粘包/拆包）       │
                         │   └─ CouponService（msgid→handler 注册表）     │
                         │       ├─ MysqlPool（8 连接池 + RAII）          │
                         │       └─ CouponRedis（thread_local 连接 + Lua）│
                         └──────────┬───────────────┬───────────────────┘
                                     │               │
                              ┌──────▼─────┐   ┌────▼─────┐
                              │   MySQL    │   │  Redis   │
                              │ coupon_db  │   │ stock/   │
                              │ 17 张表    │   │ claimed/ │
                              └────────────┘   │ used     │
                                               └──────────┘
```

## 线程模型（与自研库的配合）

| 层 | 线程 | 职责 |
|---|---|---|
| IO 层 | baseloop + N 个 sub-reactor | epoll ET 读写、accept；`send` 跨线程时 queueInLoop 到 IO 线程发送 |
| 业务层 | BusinessThreadPool（8） | 库把每次 read 的数据拷贝成独立 Buffer 投递到业务线程池，**消息回调在业务线程执行** |
| 存储层 | — | MySQL 用连接池（MysqlConn RAII，用完即还）；Redis 用 **thread_local 连接**（hiredis context 非线程安全） |

因此业务回调里可以直接做 MySQL/Redis 阻塞调用，不卡 IO 线程；
同一连接的粘包/拆包由 **Codec（per-连接累积缓冲 + 互斥锁）** 解决——因为同一连接相邻 read
可能被不同业务线程并发回调，Codec 在锁内切包、锁外回调，保证先到先解析。

## 协议

`[4 字节大端长度头][JSON 载荷]`，载荷统一携带 `msgid`：

| msgid | 命令 | 请求字段 |
|---|---|---|
| 1 | 发券 issue | title, total, [start_time/end_time] |
| 2 | 领券 claim | coupon_id, user_id |
| 3 | 核销 use | user_id, coupon_code |
| 4 | 我的券 query_mine | user_id |
| 5 | 券信息 query | coupon_id |

响应统一：`{"msgid", "errno", "msg", "data"}`。

## 核心设计

### 1. 防超发：Redis Lua 原子脚本（第一道闸）

```lua
-- KEYS[1]=coupon:stock:{id}  KEYS[2]=coupon:claimed:{id}  ARGV[1]=userId
local stock = redis.call('GET', KEYS[1])
if not stock then return -3 end          -- 券不存在
stock = tonumber(stock)
if stock <= 0 then return -1 end         -- 库存不足
if redis.call('SISMEMBER', KEYS[2], ARGV[1]) == 1 then return -2 end  -- 重复领取
redis.call('DECR', KEYS[1])
redis.call('SADD', KEYS[2], ARGV[1])
return stock - 1                         -- 剩余库存
```

Redis 单线程执行脚本，脚本内 GET/比较/DECR/SADD 天然原子，**并发下不会超发**。
比"DECR 后检查再回滚"的非原子方案严谨，比 WATCH 乐观锁在争用下更快。

### 2. 防重复领取：Redis SADD + MySQL 唯一索引（双保险）

- 第一道：Lua 里 `SISMEMBER` 判重 + `SADD` 落标记；
- 第二道：`user_coupon_{shard}` 建 `UNIQUE KEY uk_user_coupon(user_id, coupon_id)`，
  即使 Redis 数据丢失/崩溃恢复期，DB 也拒绝同一用户领同一券。

### 3. 一致性补偿：MySQL 落库失败回滚 Redis

```
Lua 预扣成功 → INSERT user_coupon 失败（唯一冲突/DB 错误）
           → INCR 库存 + SREM 用户（claimRollback）
           → 返回错误
```
避免"Redis 扣了、DB 没记"的库存黑洞。

### 4. 核销幂等：MySQL 原子 UPDATE

```sql
UPDATE user_coupon_9 SET status=1, used_at=NOW(), order_id='...'
 WHERE coupon_code='C6-1001-xxx' AND user_id=1001 AND status=0
```
- `affectedRows == 1`：核销成功，写 Redis `coupon:used:{code}` 快速缓存；
- `affectedRows == 0`：查 status —— `1` 返回"已核销"（**幂等**），无记录返回"券无效"。
并发重复核销由 InnoDB 行锁串行化，天然安全。

### 5. 分表：user_coupon_{0..15}

- 路由：`table = "user_coupon_" + (userId % 16)`；
- 同一用户所有券落在同一张表：查询"我的券"只查一张表、唯一索引冲突局域化；
- 写热点分散到 16 张表，缓解单表锁竞争与 IO 压力；
- 分表键 = 用户 ID（业务访问按用户维度），无跨表事务。

## 对自研库 Mymuduo 的修复（本次发现并修复）

| 问题 | 现象 | 修复 |
|---|---|---|
| `TcpConnnction::send` 跨线程分支**悬垂指针**（`std::bind` 只捕获 `spMsg->data()` 裸指针，未持有 `shared_ptr`） | 高频下响应头出现垃圾字节 `0x02801dff`，客户端报 bad frame | 改用 lambda 捕获 `shared_ptr`，保证字符串与连接生命周期延续到发送完成 |
| `Socket::Accept` 批量 accept 时**所有 fd 共用最后一次 accept 的 peer 地址** | 同一客户端端口名重复 established，连接表覆盖混乱 | `Accept` 返回 `{fd, peer}` 结构体，每个 fd 携带自己的对端地址 |
| `RedisClient` **无锁并发**使用同一 hiredis context（多个 IO 线程 ZADD + baseloop 定时器 ZRANGEBYSCORE/ZREM） | 高并发偶发丢请求(≈10%)、服务假死、Broken pipe 风暴 | 所有命令加内部互斥锁（hiredis context 非线程安全） |
| 端口硬编码 8081 | 部署不便 | `Tcpserver/Acceptor/Socket` 增加端口参数（默认 8081 向后兼容） |

## 目录结构

```
Mymuduo_coupon/
├── CMakeLists.txt
├── README.md
├── sql/init.sql           # 建库 + coupon 表 + 16 张分表 + 业务账号
├── redis/claim_coupon.lua # 防超发 Lua 脚本（与源码内嵌一致）
├── src/
│   ├── protocol.{hpp,cc}      # 4字节长度头 + JSON 封包/解包
│   ├── codec.{hpp,cc}         # per-连接累积解码器（锁内切包，锁外回调）
│   ├── mysqlpool.{hpp,cc}     # MySQL 连接池 + ResultSet + RAII + 分表路由
│   ├── rediscoupon.{hpp,cc}   # 业务 Redis 客户端（thread_local + Lua + 补偿）
│   ├── couponservice.{hpp,cc} # 业务单例 + msgid 注册表
│   ├── couponserver.{hpp,cc}  # Tcpserver 封装 + Codec 生命周期管理
│   └── main.cc
└── client/client.cc       # 压测/演示客户端（ping/issue/claim/use/mine/info/
                           #   stress/dup/stress-mix）
```

## 编译运行

```bash
# 0) 依赖：MySQL 8.x、Redis、libmysqlclient、libhiredis、nlohmann-json
sudo apt-get install -y mysql-server redis-server libhiredis-dev nlohmann-json3-dev libmysqlclient-dev

# 1) 先编译自研库（Mymuduo 项目）
cd ../mymudo/build && cmake .. && make -j4

# 2) 初始化数据库（建库 + 17 张表 + 账号 coupon/coupon123）
mysql -uroot < sql/init.sql

# 3) 编译并启动优惠券服务器（支持参数：./couponserver [port] [businessThreads]，默认 9081 / 8）
mkdir -p build && cd build && cmake .. && make -j4
LD_LIBRARY_PATH=../mymudo/lib ./couponserver 9081 8

# 4) 压测/演示
./couponclient issue "秒杀券" 50              # 发券
./couponclient stress <coupon_id> 300 16     # 300 并发领 50 张 → 恰 50 成功
./couponclient dup <coupon_id> <user> 100    # 同用户并发 100 次 → 1 成功 99 重复
./couponclient stress-mix <coupon_id> 50 8   # 领券+双核销 → 核销幂等
```

## 优惠券工程优化记录（新增）

| 优化点 | 背景问题 | 做法 | 验证 |
|---|---|---|---|
| 领券唯一索引冲突幂等化 | 若 Redis 数据丢失（重启/FLUSHALL）而 MySQL 已有领取记录，原逻辑落库失败时一律 `SREM` 移除 claimed 集合，用户会**反复走"可领取→唯一索引冲突→再移除"循环**，永远无法 duplicate 且反复打 DB | 区分 MySQL 错误号：`ER_DUP_ENTRY(1062)` 时保持 claimed 集合（用户已在其中），仅回补库存并按 `ERR_DUP_CLAIM` 幂等返回；其他错误才完整回滚 | 模拟 `DEL coupon:claimed:{id}` 后：第 2 次返回 errno=3（db unique index）且库存回补一致，第 3 次在 Redis 层直接 duplicate，库存/DB 行数均正确 |
| MySQL 连接池取连接有限等待 | 空闲连接耗尽时 `doGet` 立即返回空，高并发瞬时无连接可用的请求直接报 `ERR_DB` | `doGet` 改为条件变量等待（默认 3s），归还时 `notify_one` 唤醒 | 200 并发抢 50 库存（8 业务线程/8 连接）：成功 50 / 无库存 150 / 其他错误 0 / 连接失败 0 |
| 服务端端口/线程数参数化 | 端口与业务线程数硬编码，部署不同环境需改代码 | `./couponserver [port] [businessThreads]`（默认 9081/8） | 以 `./couponserver 9081 8` 启动正常 |

## 端到端验证结果（本机实测）

| 场景 | 结果 |
|---|---|
| 300 并发 ping（修复后回归） | 300/300 成功，0 失败 |
| 发券 total=50，300 用户×16 线程并发领 | **成功 50 / 库存不足 250 / 重复 0 / 异常 0**（零超发） |
| 同用户并发领 100 次 | 成功 1 / 重复 99（去重生效） |
| 50 用户领券 + 每人核销 2 次 | 领券 50 成功；首次核销全成功、二次核销全幂等返回"已核销" |
| 数据一致性 | MySQL `user_coupon_{uid%16}` 记录与 Redis `stock/claimed` 实时一致 |

## 面试可讲点

1. **为什么 Lua**：Redis 单线程执行脚本 = 原子性，避免 DECR 后回滚的非原子竞态；
2. **双保险**：Redis 做热路径，MySQL 唯一索引做最终一致兜底，含失败补偿回滚；
3. **分表**：哈希路由 + 查询亲和 + 冲突局域化，16 张表可平滑扩展；
4. **线程模型**：IO 线程只做读写，业务线程池消化 DB/Redis 阻塞，`send` 跨线程安全；
5. **过程中修的库 bug**（悬垂指针/accept peer/RedisClient 并发）本身就是"高并发下发现并修复网络库缺陷"的实战故事。
