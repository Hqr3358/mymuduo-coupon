#!/usr/bin/env bash
# 一键构建 + 冒烟测试
# 用法: scripts/build_and_test.sh
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "==> [1/4] cmake"
mkdir -p build && cd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .. > /tmp/coupon_cmake.log 2>&1 || {
  echo "cmake failed, see /tmp/coupon_cmake.log"; tail -30 /tmp/coupon_cmake.log; exit 1;
}

echo "==> [2/4] make"
make -j"$(nproc)" > /tmp/coupon_make.log 2>&1 || {
  echo "make failed, see /tmp/coupon_make.log"; tail -40 /tmp/coupon_make.log; exit 1;
}

echo "==> [3/4] 产物检查"
SERVER=./Mymuduo_coupon/couponserver
CLIENT=./Mymuduo_coupon/couponclient
[ -x "$SERVER" ] || { echo "missing $SERVER"; exit 1; }
[ -x "$CLIENT" ] || { echo "missing $CLIENT"; exit 1; }
echo "  server: $SERVER"
echo "  client: $CLIENT"

echo "==> [4/4] 冒烟（ping）"
# 服务端需要 Redis/MySQL 在跑；没起就只做构建检查
if ! pgrep -x mysqld > /dev/null || ! pgrep -x redis-server > /dev/null; then
  echo "  (mysqld/redis-server 未运行，跳过 ping，构建通过)"
  exit 0
fi

LD_LIBRARY_PATH="$ROOT/Mymuduo/lib" "$SERVER" 9081 8 &
SPID=$!
trap "kill $SPID 2>/dev/null" EXIT
sleep 1

if "$CLIENT" ping | grep -q '"pong"'; then
  echo "  ping OK"
else
  echo "  ping FAILED"
  exit 1
fi
echo "==> done"
