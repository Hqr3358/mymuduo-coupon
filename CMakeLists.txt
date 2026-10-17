cmake_minimum_required(VERSION 3.10)
project(mymuduo_coupon_system)

# ============================================================
# 优化：根级总构建入口——将自研 muduo 网络库与优惠券系统
#       作为一个整体项目一次性构建（自研muduo网络 + Redis + MySQL + 优惠券业务）
#   用法（仓库根目录执行）：
#       mkdir -p build && cd build
#       cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
#       make -j4
#   产物：
#       Mymuduo/lib/libmymuduo.so            （自研网络库动态库）
#       build/Mymuduo_coupon/couponserver    （优惠券服务端，监听 9081）
#       build/Mymuduo_coupon/couponclient    （压测/演示客户端）
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 子项目①：自研 muduo 网络库（add_library(mymuduo SHARED)）
add_subdirectory(Mymuduo)
# 子项目②：优惠券系统（链接 mymuduo 目标 + hiredis + mysqlclient + nlohmann-json）
add_subdirectory(Mymuduo_coupon)
