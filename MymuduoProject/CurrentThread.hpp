#pragma once
#include<sys/syscall.h>
#include<unistd.h>
namespace CurrentThread
{
    extern __thread int selftid;//声明selftid并且每个线程独立
    void cacheTid();
    inline int tid()
    {
		if(__builtin_expect(selftid==0,0))
        {
			cacheTid();
        }
        return selftid;
    } 
} 