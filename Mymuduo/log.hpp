#pragma  once
#include"TimeStamp.hpp"
#include <iostream>
#include <unistd.h>
#include <stdarg.h>
#include <string>
enum
{
    FATAL=-1,
    DEBUG,
    NORMOL,
    ERR
};
inline std::string To_str(int level)
{
    std::string str="";
    switch (level)
    {
    case DEBUG:
        str = "DEBUG";
        break;
    case NORMOL:
        str = "NORMOL";
        break;
    case ERR:
        str = "ERR";
        break;
    case FATAL:
        str = "FATAL";
        break;
    default:
        break;
    }
    return str;
}
inline void Logmanage(int level,std::string message, ...)
{
    char logprefix[1024];
    TimeStamp now;
    std::string str = To_str(level);
    if(str=="FATAL")
    {
        sprintf(logprefix, "[%s][%s][pid: %d]", str.c_str(), now.to_string().c_str(), getpid());
    	char logcontent[1024];
    	va_list arg; // 可变参
   	 	va_start(arg,message);
    	vsprintf(logcontent, message.c_str(), arg);
        std::cout<<logprefix<<"->"<<logcontent<<std::endl;
		exit(FATAL);
    }else
    {
       sprintf(logprefix, "[%s][%s][pid: %d]", str.c_str(),now.to_string().c_str(), getpid());
    	char logcontent[1024];
    	va_list arg; // 可变参
    	va_start(arg,message);
    	vsprintf(logcontent, message.c_str(), arg); // 将可变参数具体化,就是将arg->并入message中
    	// FILE *fp1 = fopen("log.txt", "wa");
    	// FILE *fp2 = fopen("log.err", "wa");
        if(str=="ERR") 
        {
            std::cerr<<logprefix<<"->"<<logcontent<<std::endl; 
        }else std::cout<<logprefix<<"->"<<logcontent<<std::endl; 	
    }
}