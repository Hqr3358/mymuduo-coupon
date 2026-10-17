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
/** @brief 构造：保存主循环指针，线程数默认 0（单线程模式） */
	explicit EventLoopThreadPool(EventLoop* baseloop);
/** @brief 析构：默认 */
    ~EventLoopThreadPool();
/** @brief 启动线程池：按线程数创建 EventLoopThread */
    void start(const ThreadInitCallback& cb=ThreadInitCallback());
/** @brief 设置 IO 线程数 */
    void Setnumthread(int numthread);
/** @brief 轮询返回下一个 EventLoop（负载均衡：新连接分派） */
    EventLoop* getNextLoop();
private:
	std::atomic_bool started_;
	EventLoop* baseloop_;
	std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
    int numthread_;
    static int next_;
};