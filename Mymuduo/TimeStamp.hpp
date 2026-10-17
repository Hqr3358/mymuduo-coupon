#pragma once
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include<time.h>
#include<string>
/** @brief 时间戳：封装 time_t 与格式化时间字符串 */
class TimeStamp
{
public:
	TimeStamp():timer_(time(nullptr)){}
/** @brief 返回格式化时间字符串（YYYY-MM-DD HH:MM:SS） */
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
/** @brief 返回当前 Unix 时间戳 */
    static time_t now()//返回当前时间戳
    {
		return time(NULL);
    }
private:
	time_t timer_;;
};