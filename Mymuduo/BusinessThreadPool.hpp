#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// 业务线程池：把业务逻辑（消息处理、DB/Redis 调用等）从 IO 线程剥离出去，
// IO 线程只负责 epoll_wait + read/write，不被业务逻辑阻塞。
class BusinessThreadPool
{
public:
    using Task = std::function<void()>;

    // maxQueueSize：任务队列最大长度，超过就拒绝新任务，防止内存OOM
    explicit BusinessThreadPool(int numThreads = 4, size_t maxQueueSize = 2000);
    ~BusinessThreadPool();

    BusinessThreadPool(const BusinessThreadPool&) = delete;
    BusinessThreadPool& operator=(const BusinessThreadPool&) = delete;

    // 启动 N 个工作线程
    void start();
    // 停止并等待所有工作线程退出
    void stop();
    // 投递一个任务到队列，由空闲工作线程执行
    // 返回 true=入队成功；false=队列已满，任务被拒绝（限流）
    bool enqueue(Task task);
    // 获取当前队列积压任务数（监控负载用）
    size_t getQueueSize();

private:
    void workerLoop();

    int numThreads_;
    // 任务队列最大容量，超过就拒绝
    size_t maxQueueSize_;
    std::vector<std::thread> workers_;
    std::queue<Task> taskQueue_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic_bool running_;
};
