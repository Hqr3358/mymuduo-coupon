# mymuduo-coupon

基于**自研 Mymuduo 网络库**（仿 muduo）的完整优惠券业务系统：**发券 / 领券 / 核销**，Redis 防超发 + MySQL 唯一索引/16 分表。

## 目录结构

```
mymuduo-coupon/
└── Mymuduo_coupon/          # 优惠券系统（本仓库全部内容）
    ├── src/                 # 协议、解码器、MySQL 连接池、Redis 客户端、业务服务
    ├── client/              # 压测/演示客户端（纯 POSIX socket）
    ├── sql/                 # 建表 + 16 分表 DDL
    ├── redis/               # 防超发 Lua 脚本
    └── CMakeLists.txt
```

> 自研网络库（`Mymuduo`）为**独立仓库**：`github.com/Hqr3358/Mymuduo`，本工程编译时通过 `MYMUDUO_DIR` 引用（默认取上级目录 `../Mymuduo`，可 `-DMYMUDUO_DIR=` 覆盖）。

## 快速开始

```bash
# 1. 拉取网络库（放在本仓库同级目录，默认路径 ../Mymuduo）
git clone git@github.com:Hqr3358/Mymuduo.git

# 2. 编译网络库
cd Mymuduo && mkdir -p build && cd build \
  && cmake .. && make -j4   # 产物：lib/libmymuduo.so

# 3. 编译优惠券系统
cd ../Mymuduo_coupon && mkdir -p build && cd build \
  && cmake .. && make -j4   # 若网络库在其他路径：cmake -DMYMUDUO_DIR=... ..

# 4. 启动（Redis/MySQL 就绪后）
LD_LIBRARY_PATH=../Mymuduo/lib ./couponserver
```

详细功能、验证结果与面试讲解见 [Mymuduo_coupon/README.md](Mymuduo_coupon/README.md)。
