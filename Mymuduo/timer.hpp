#include <cstdint>
#include <functional>
#include <sys/timerfd.h>
#include <unistd.h>
#include"Channel.hpp"
class EventLoop;
/** @brief 创建 timerfd 并设置 3 秒周期触发（定时器底层 fd） */
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
/** @brief 定时器封装：timerfd + Channel，周期触发超时回调（超时清理驱动） */
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
/** @brief 设置超时回调（如 Tcpserver::handleTimeout） */
	void SettimeoutCallback(timeoutCallback&& cb)
	{
		timeoutCallback_=std::move(cb); 
	}
/** @brief 更新轮询周期：首次 timeout 秒后触发，此后每 timeout 秒循环 */
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
/** @brief timerfd 可读处理：读取计数并触发超时回调 */
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