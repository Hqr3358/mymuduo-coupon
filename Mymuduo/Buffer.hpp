#pragma once
#include <cstddef>
#include <sys/types.h>
#include <vector>
#include <string>
class Buffer
{
    static const size_t kCheapPrePend=8;
    static const size_t kInitialSize=1024;
public:
	Buffer(size_t InitialSize=kInitialSize);
    ~Buffer(){}
    size_t writeableBytes() const//可写入区域长度
    {
        return buffer_.size()-writerIndex_;
    }
    size_t readableBytes() const//能读取区域长度
    {
        return writerIndex_-readerIndex_;
    }
    size_t prePendableBytes() const//前置区域长度
    {
        return readerIndex_;
    }
	const char* peek() const//窥探可读取数据的起始位置
    {
        return begin()+readerIndex_;
    }
	void retrieve(size_t len);//将buffer里读取了len长度的可读数据去掉
    void retrieveAll();//将buffer里全部可读数据去掉
    std::string retrieveAllAsString();//将buffer里全部可读数据char变成string并读取出来
    std::string retrieveAsString(size_t len);//将buffer里len长度可读数据char变成string并读取出来
    void enSurewriteableBytes(size_t len);//判断可写入长度或者前移后是否够len长，不够就扩容
    void append(const char* data,size_t len);//将data，data＋len区域拷贝到写入区域，不够就扩容
    ssize_t readFd(int fd,int* saveErrno);//提取fd获得的数据
    ssize_t writeFd(int fd,int* saveErrno);
private:
	char* begin()
    {
        return &*buffer_.begin();
    }
    const char* begin() const
    {
        return &*buffer_.begin();
    }
    void makeSpace(size_t len);//扩容
	std::vector<char> buffer_;//接收缓冲区，分成三个区域－>>前置区域｜可读取数据区域｜可写入区域
    size_t readerIndex_;//可读取数据的起始位置
    size_t writerIndex_;//可写入数据的起始位置
};