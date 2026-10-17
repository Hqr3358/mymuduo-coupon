#include <atomic>
#include <functional>
#include <memory>
#include<thread>
/** @brief 线程封装：pthread 创建 + 信号量就绪同步（EventLoopThread 底层） */
class Thread
{
    using Func=std::function<void()>;
public:
/** @brief 构造线程对象：保存入口函数（实际线程在 start() 创建） */
	explicit Thread(Func fun);
/** @brief 析构：默认 */
    ~Thread();
/** @brief 启动线程（创建 pthread，等待就绪信号后返回） */
    void start();
/** @brief 等待线程退出 */
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