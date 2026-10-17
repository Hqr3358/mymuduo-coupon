#include "couponserver.hpp"
#include "log.hpp"

#include <csignal>
#include <thread>

// 优雅退出：SIGINT / SIGTERM
static void handleSignal(int sig)
{
    Logmanage(NORMOL, "signal %d received, couponserver exiting...", sig);
    exit(0);
}

int main()
{
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);
    signal(SIGPIPE, SIG_IGN); // send 已用 MSG_NOSIGNAL，此处双保险

    EventLoop loop;

    // 第二个参数为监听端口；自研库 Tcpserver 已支持端口可配置（默认 8081）
    CouponServer server(&loop, 9081);
    server.SetThreadnum(std::thread::hardware_concurrency()); // sub-reactor IO 线程
    server.SetBusinessThreadNum(8);                           // 业务线程池（DB/Redis 阻塞调用）
    server.start();

    Logmanage(NORMOL, "couponserver listening on 9081");
    loop.loop();
    return 0;
}
