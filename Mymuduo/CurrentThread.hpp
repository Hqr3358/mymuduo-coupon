#pragma once
#include<sys/syscall.h>
#include<unistd.h>
namespace CurrentThread
{
    extern __thread int selftid;//声明selftid并且每个线程独立
/** @brief 缓存当前线程 tid（thread_local，避免频繁 syscall） */
    void cacheTid();
/** @brief 获取当前线程 tid（首次调用时缓存） */
    inline int tid()
    {
		if(__builtin_expect(selftid==0,0))
        {
			cacheTid();
        }
        return selftid;
    } 
} 