#include <atomic>
#include <functional>
#include <memory>
#include<thread>
class Thread
{
    using Func=std::function<void()>;
public:
	explicit Thread(Func fun);
    ~Thread();
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