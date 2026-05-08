#pragma once
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include<time.h>
#include<string>
class TimeStamp
{
public:
	TimeStamp():timer_(time(nullptr)){}
	std::string to_string() const//返回具体时间
    {
        char buff[128];
        memset(buff, 0, sizeof(buff));
        tm* time=localtime(&timer_);
		sprintf(buff,"%04d-%02d-%02d %02d:%02d:%02d"
        ,time->tm_year+1900
        ,time->tm_mon+1
    	,time->tm_mday
    	,time->tm_hour
    	,time->tm_min
    	,time->tm_sec);
        return buff;
    }
    static time_t now()//返回当前时间戳
    {
		return time(NULL);
    }
private:
	time_t timer_;;
};