#include <cstdint>
#include <functional>
#include <sys/timerfd.h>
#include <unistd.h>
#include"Channel.hpp"
class EventLoop;
static int createTimer()
{
    int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	struct itimerspec spec;
	// 第一次 3 秒后触发
	spec.it_value.tv_sec  = 3;
	spec.it_value.tv_nsec = 0;
	// 之后每 3 秒循环一次
	spec.it_interval.tv_sec  = 3;
	spec.it_interval.tv_nsec = 0;
	// 生效
	timerfd_settime(timerfd, 0, &spec, nullptr);
	return timerfd;
}
class Timer
{
	using timeoutCallback=std::function<void()>;
public:
	Timer(EventLoop* loop):loop_(loop),timerfd_(createTimer())
	,timerchannel_(loop,timerfd_)
	{
		timerchannel_.SetrecvCallback(std::bind(&Timer::handleRead,this));
		timerchannel_.SetEnableIn();
	}
	void SettimeoutCallback(timeoutCallback&& cb)
	{
		timeoutCallback_=std::move(cb); 
	}
	void updatetimeoutPolling(int timeout)
	{
		struct itimerspec spec;
		// 第一次 timeout 秒后触发
		spec.it_value.tv_sec  = timeout;
		spec.it_value.tv_nsec = 0;
		// 之后每 timeout 秒循环一次
		spec.it_interval.tv_sec  = timeout;
		spec.it_interval.tv_nsec = 0;
		// 生效
		timerfd_settime(timerfd_, 0, &spec, nullptr);
	}
	void handleRead()
	{
		uint64_t exp;
		read(timerfd_, &exp, sizeof(exp));
		timeoutCallback_();
	}
private:
	EventLoop* loop_;
	int timerfd_;
	Channel timerchannel_;
	timeoutCallback timeoutCallback_;

};