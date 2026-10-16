#include "BusinessThreadPool.hpp"
#include "log.hpp"

BusinessThreadPool::BusinessThreadPool(int numThreads)
    : numThreads_(numThreads)
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
    Logmanage(NORMOL, "business thread pool started, thread num:%d", numThreads_);
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

void BusinessThreadPool::enqueue(Task task)
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        taskQueue_.push(std::move(task));
    }
    cv_.notify_one();
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
