#include <atomic>
#include <functional>
#include <memory>
#include<thread>
/** @brief 线程封装：pthread 创建 + 信号量就绪同步（EventLoopThread 底层） */
class Thread
{
    using Func=std::function<void()>;
public:
	// 构造：保存线程入口函数
	explicit Thread(Func fun);
    // 析构
    ~Thread();
    // 启动线程
    void start();
    void join()
    {
        joined_=true;
        thread_->join();
    }
private:
	pid_t tid_;
    std::atomic_bool started_;
    std::atomic_bool joined_;
    std::unique_ptr<std::thread> thread_;
	Func func_;
};