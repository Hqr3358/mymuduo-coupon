#include "EventLoop.hpp"
#include "EventLoopThread.hpp"
#include <atomic>
#include <memory>
#include<vector>
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