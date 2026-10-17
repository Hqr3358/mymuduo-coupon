#include "couponserver.hpp"
#include "log.hpp"

#include <csignal>
#include <cstdlib>
#include <thread>

// 优雅退出：SIGINT / SIGTERM
static void handleSignal(int sig)
{
    Logmanage(NORMOL, "signal %d received, couponserver exiting...", sig);
    exit(0);
}

int main(int argc, char** argv)
{
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);
    signal(SIGPIPE, SIG_IGN); // send 已用 MSG_NOSIGNAL，此处双保险

    // 新增：监听端口与业务线程数支持命令行参数，便于不同环境部署：
    //   ./couponserver [port] [businessThreads]，默认 9081 / 8
    uint16_t port = 9081;
    int bizThreads = 8;
    if (argc >= 2)
    {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }
    if (argc >= 3)
    {
        bizThreads = std::atoi(argv[2]);
        if (bizThreads <= 0) bizThreads = 8;
    }

    EventLoop loop;

    // 第二个参数为监听端口；自研库 Tcpserver 已支持端口可配置（默认 8081）
    CouponServer server(&loop, port);
    server.SetThreadnum(std::thread::hardware_concurrency()); // sub-reactor IO 线程
    server.SetBusinessThreadNum(bizThreads);                 // 业务线程池（DB/Redis 阻塞调用）
    server.start();

    Logmanage(NORMOL, "couponserver listening on %d", port);
    loop.loop();
    return 0;
}
