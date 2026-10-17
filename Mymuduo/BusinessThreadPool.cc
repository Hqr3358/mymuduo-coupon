#include "BusinessThreadPool.hpp"
#include "log.hpp"

BusinessThreadPool::BusinessThreadPool(int numThreads, size_t maxQueueSize)
    : numThreads_(numThreads)
    // 初始化队列上限
    , maxQueueSize_(maxQueueSize)
    , running_(false)
{
}

BusinessThreadPool::~BusinessThreadPool()
{
    stop();
}

void BusinessThreadPool::start()
{
    running_ = true;
    for (int i = 0; i < numThreads_; ++i)
    {
        workers_.emplace_back([this]() { workerLoop(); });
    }
    // 启动日志里带上队列上限
    Logmanage(NORMOL, "business thread pool started, thread num:%d, max queue size:%zu",
              numThreads_, maxQueueSize_);
}

void BusinessThreadPool::stop()
{
    running_ = false;
    cv_.notify_all();
    for (auto& t : workers_)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
    workers_.clear();
}

// 入队加限流：队列满直接拒绝，不阻塞IO线程
bool BusinessThreadPool::enqueue(Task task)
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        // 队列长度达到上限，拒绝新任务，打印日志
        if (taskQueue_.size() >= maxQueueSize_)
        {
            Logmanage(ERR, "business pool queue full! size:%zu max:%zu, reject task",
                      taskQueue_.size(), maxQueueSize_);
            return false;
        }
        taskQueue_.push(std::move(task));
    }
    cv_.notify_one();
    return true;
}

// 返回当前队列积压数，给定时器/监控用
size_t BusinessThreadPool::getQueueSize()
{
    std::lock_guard<std::mutex> lock(mtx_);
    return taskQueue_.size();
}

void BusinessThreadPool::workerLoop()
{
    while (running_)
    {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]() { return !running_ || !taskQueue_.empty(); });
            if (!running_ && taskQueue_.empty())
            {
                return;
            }
            task = std::move(taskQueue_.front());
            taskQueue_.pop();
        }
        try
        {
            task();
        }
        catch (const std::exception& e)
        {
            Logmanage(ERR, "business task exception: %s", e.what());
        }
    }
}
