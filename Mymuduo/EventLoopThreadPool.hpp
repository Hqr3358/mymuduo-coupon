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
	// 构造：保存主循环指针
	explicit EventLoopThreadPool(EventLoop* baseloop);
    // 析构
    ~EventLoopThreadPool();
    // 启动 IO 线程池
    void start(const ThreadInitCallback& cb=ThreadInitCallback());
    // 设置 IO 线程数
    void Setnumthread(int numthread);
    // 轮询取下一个 EventLoop（新连接分派）
    EventLoop* getNextLoop();
private:
	std::atomic_bool started_;
	EventLoop* baseloop_;
	std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
    int numthread_;
    static int next_;
};