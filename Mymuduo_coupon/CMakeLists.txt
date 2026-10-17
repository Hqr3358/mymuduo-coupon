cmake_minimum_required(VERSION 3.10)
project(mymuduo_coupon)

# ============ 基于自研 muduo 网络库的优惠券系统 ============
# 依赖：libmymuduo.so（自研网络库）、libhiredis、libmysqlclient、nlohmann-json
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wextra -pthread")

# ---- 自研 muduo 网络库（头文件目录 + 已编译的动态库）----
# 网络库为独立仓库 mymudo，默认位于本工程上一级目录 ../mymudo；
# 若网络库在其他位置，编译时用 -DMYMUDUO_DIR=/path/to/mymudo 覆盖。
if(NOT DEFINED MYMUDUO_DIR)
    set(MYMUDUO_DIR ${CMAKE_SOURCE_DIR}/../mymudo)
endif()
include_directories(${MYMUDUO_DIR})
link_directories(${MYMUDUO_DIR}/lib)

# ---- 系统依赖 ----
include_directories(/usr/include/mysql /usr/include/hiredis /usr/include/nlohmann)
find_library(MYSQL_LIB NAMES mysqlclient PATHS /usr/lib/x86_64-linux-gnu /usr/lib64/mysql)
find_library(HIREDIS_LIB NAMES hiredis PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib)
find_package(Threads REQUIRED)

# ============ 服务端 ============
set(SERVER_SRC
    src/protocol.cc
    src/codec.cc
    src/mysqlpool.cc
    src/rediscoupon.cc
    src/couponservice.cc
    src/couponserver.cc
    src/main.cc)
add_executable(couponserver ${SERVER_SRC})
target_link_libraries(couponserver mymuduo ${MYSQL_LIB} ${HIREDIS_LIB} Threads::Threads)

# ============ 压测/演示客户端（纯 POSIX socket，不依赖网络库）============
add_executable(couponclient client/client.cc)
target_link_libraries(couponclient Threads::Threads)
