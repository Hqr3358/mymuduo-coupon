#include"Buffer.hpp"
#include <algorithm>
#include <cerrno>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
Buffer::Buffer(size_t InitialSize):
buffer_(kCheapPrePend+InitialSize)
,readerIndex_(kCheapPrePend)
,writerIndex_(kCheapPrePend)
{}
void Buffer::retrieve(size_t len)
{
	if(len<readableBytes())
    {
        readerIndex_+=len;
    }else
    {
        retrieveAll();
    }
}
void Buffer::retrieveAll()
{
	readerIndex_=writerIndex_=kCheapPrePend;
}
std::string Buffer::retrieveAllAsString()
{
    return retrieveAsString(readableBytes());
}
std::string Buffer::retrieveAsString(size_t len)
{
	std::string result(peek(),len);
    retrieve(len);
	return result;
}
void Buffer::makeSpace(size_t len)
{
    if(writeableBytes()+prePendableBytes()-kCheapPrePend<len)
    {
        buffer_.resize(writerIndex_+len);
    }else
    {
        size_t readable=readableBytes();
        std::copy(begin()+readerIndex_,begin()+readerIndex_+readable,begin()+kCheapPrePend);
        readerIndex_=kCheapPrePend;
        writerIndex_=readerIndex_+readable;
    }
}
void Buffer::enSurewriteableBytes(size_t len)
{
	if(writeableBytes()<len)
    {
        makeSpace(len);
    }
}
void Buffer::append(const char* data,size_t len)
{
	enSurewriteableBytes(len);
    std::copy(data,data+len,begin()+writerIndex_);
	writerIndex_+=len;
}
ssize_t Buffer::readFd(int fd,int* saveErrno)
{
    /*struct iovec
  {
    void *iov_base;	 Pointer to data.  
    size_t iov_len;	 Length of data.  
  };
  	*/
    char extrabuf[65536];
    struct iovec iovc[2];
    size_t writeable=writeableBytes();
    iovc[0].iov_base=begin()+writerIndex_;
	iovc[0].iov_len=writeable;
    iovc[1].iov_base=extrabuf;
	iovc[1].iov_len=sizeof(extrabuf);
    int count=2;
    ssize_t n=readv(fd,iovc,count);
    if(n<0)
    {
        *saveErrno=errno;
    }else if(n<=writeable)
    {
		writerIndex_+=n;
    }else
    {
		writerIndex_=buffer_.size();
        size_t len=n-writeable;
        append(extrabuf,len);
    }
    return n;
}
ssize_t Buffer::writeFd(int fd,int* saveErrno)
{
    ssize_t n=::send(fd,begin()+readerIndex_,readableBytes(),MSG_NOSIGNAL);
    if(n<0) *saveErrno=errno;
    return n;
}
