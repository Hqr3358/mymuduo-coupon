#include"poller.hpp"
#include"Eventepoller.hpp"
Poller::~Poller()=default;
Poller* Poller::Newepoller(EventLoop* loop)
{
    return new Eventepoller(loop);
}
