#include"poller.hpp"
#include"Eventepoller.hpp"
Poller::~Poller()=default;
/** @brief 工厂：创建 epoll 版 Poller（EventLoop 构造时调用） */
Poller* Poller::Newepoller(EventLoop* loop)
{
    return new Eventepoller(loop);
}
