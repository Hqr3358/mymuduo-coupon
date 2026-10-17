#include"CurrentThread.hpp"

namespace CurrentThread 
{
	__thread int selftid=0;
/** @brief 缓存当前线程 tid（thread_local，避免频繁 syscall） */
    void cacheTid()
    {
        if(selftid==0)
        	selftid=static_cast<pid_t>(syscall(SYS_gettid));
    }
}