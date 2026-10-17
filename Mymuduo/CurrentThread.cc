#include"CurrentThread.hpp"

namespace CurrentThread 
{
	__thread int selftid=0;
    void cacheTid()
    {
        if(selftid==0)
        	selftid=static_cast<pid_t>(syscall(SYS_gettid));
    }
}