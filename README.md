# mymuduo-coupon

基于**自研 Mymuduo 网络库**（仿 muduo）的完整优惠券业务系统：**发券 / 领券 / 核销**，Redis 防超发 + MySQL 唯一索引/16 分表。

## 目录结构（单仓库自包含）

```
mymuduo-coupon/
├── Mymuduo/                # 自研 muduo 网络库源码（已并入本仓库，无需单独拉取）
│   ├── EventLoop / Acceptor / TcpConnnection / Tcpserver / BusinessThreadPool ...
│   └── CMakeLists.txt
└── Mymuduo_coupon/         # 优惠券系统
    ├── src/                # 协议、解码器、MySQL 连接池、Redis 客户端、业务服务
    ├── client/             # 压测/演示客户端（纯 POSIX socket）
    ├── sql/                # 建表 + 16 分表 DDL
    ├── redis/              # 防超发 Lua 脚本
    └── CMakeLists.txt
```

> 网络库源码同步自 `github.com/Hqr3358/mymudo`（含悬垂指针/peer 地址/Redis 线程安全修复、端口可配置、Doxygen 注释）。
> 编译时 `MYMUDUO_DIR` 默认取仓库内置的 `../Mymuduo`，也兼容外部 `../mymudo` 布局，或 `-DMYMUDUO_DIR=` 覆盖。

## 快速开始（整体构建，一次 cmake + make）

```bash
# 1. 拉取本仓库（网络库已在仓库内）
git clone git@github.com:Hqr3358/mymuduo-coupon.git

# 2. 整体构建（自研muduo网络库 + Redis + MySQL + 优惠券，一个工程一次构建）
cd mymuduo-coupon && mkdir -p build && cd build \
  && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .. \
  && make -j4
# 产物：../Mymuduo/lib/libmymuduo.so（网络库）、./Mymuduo_coupon/couponserver（服务端）、couponclient（客户端）

# 3. 启动（Redis/MySQL 就绪后，端口/业务线程可配）
LD_LIBRARY_PATH=../Mymuduo/lib ./Mymuduo_coupon/couponserver 9081 8
```

> 也可以单独进入 `Mymuduo/`、`Mymuduo_coupon/` 各自构建（CMake 均兼容），
> 但推荐上面的根级整体构建。

详细功能、验证结果与面试讲解见 [Mymuduo_coupon/README.md](Mymuduo_coupon/README.md)。
