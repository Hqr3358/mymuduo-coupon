#include "EventLoop.hpp"
#include "EventLoopThread.hpp"
#include <atomic>
#include <memory>
#include<vector>
/** @brief IO 线程池：按线程数创建 EventLoopThread，轮询分派新连接 */
class EventLoopThreadPool
{
    using ThreadInitCallback=std::function<void(EventLoop*)>; 
public:
	explicit EventLoopThreadPool(EventLoop* baseloop);
    ~EventLoopThreadPool();
    void start(const ThreadInitCallback& cb=ThreadInitCallback());
    void Setnumthread(int numthread);
    EventLoop* getNextLoop();
private:
	std::atomic_bool started_;
	EventLoop* baseloop_;
	std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
    int numthread_;
    static int next_;
};