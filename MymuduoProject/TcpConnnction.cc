#include"TcpConnnction.hpp"
#include"Channel.hpp"
#include"Socket.hpp"
#include "EventLoop.hpp"
#include "log.hpp"
#include"TimeStamp.hpp"
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstring>
#include <string>
#include <memory>
#include <functional>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
TcpConnction::TcpConnction(int fd,EventLoop* loop,std::string name):
loop_(loop)
,name_(name)
,lasttime_(TimeStamp::now())
,connsock_(std::make_unique<Socket>(fd))
,connchannel_(std::make_unique<Channel>(loop,fd))
,state_(kConnecting)
{
	connchannel_->SetrecvCallback(std::bind(&TcpConnction::handleRead,this,std::placeholders::_1));
    connchannel_->SetWriteCallback(std::bind(&TcpConnction::handleWrite,this));
	connchannel_->SetCloseCallback(std::bind(&TcpConnction::handleClose,this));
    connchannel_->SetErrCallback(std::bind(&TcpConnction::handleErr,this));
    connsock_->SetKeepAlive(true);
    connsock_->SetTcpNoDelay(true);
}
TcpConnction::~TcpConnction(){}
EventLoop* TcpConnction::getloop()
{
    return loop_;
}
int TcpConnction::fd()
{
    return connchannel_->fd();
}
void TcpConnction::conntEstablish()
{
	state_=kConnected;
    connchannel_->tie(shared_from_this());
    connchannel_->SetEnableIn();
    conntionCallback_(shared_from_this());
}
void TcpConnction::conntDestoy()
{
    state_=kDisconnected;
    connchannel_->remove();
}
void TcpConnction::handleRead(TimeStamp ti)
{
	int savedErrno=0;
    updatelastnow();
    while(true)
    {
		ssize_t n=inbuffer_.readFd(connsock_->fd(),&savedErrno);
    	if(n>0)
    	{
            //messageCallback_回调函数一定要减去已经读取的数据
			messageCallback_(shared_from_this(),&inbuffer_,ti);
    	}
    	else if(n==0)
    	{
        	handleClose();
            break;
    	}
    	else
    	{
            if(savedErrno==EAGAIN||savedErrno==EWOULDBLOCK)
            {
                break;
            }
        	errno=savedErrno;
        	Logmanage(ERR,"tcpconntion::handleread fali :%d->%s",errno,strerror(errno) );
    		handleClose();
            break;
        }
    }
}
void TcpConnction::send(const std::string& msg)
{
	if(state_==kConnected)
    {
        if(loop_->isInLoopThread())//调用send是在该loop线程下直接发送
        {
            sendloop(msg.c_str(),msg.size());
        }
        else//不是一个线程下给该loop线程任务队列
        {
            loop_->queueInLoop(std::bind(&TcpConnction::sendloop,shared_from_this(),msg.c_str(),msg.size()));
        }
    }
}
void TcpConnction::sendloop(const char* data,int len)
{
    ssize_t writen=0,remain=len;
    if(state_!=kConnected)
    {
        Logmanage(ERR,"tcpconntion::sendloop fail");
        return;
    }
    if(!connchannel_->isWriting()&&outbuffer_.readableBytes()==0)
    {
        //能直接发就直接发不给写事件监听,并设置不会被信号中断导致进程崩溃
        writen=::send(connsock_->fd(),data,len,MSG_NOSIGNAL);
        if(writen>=0)
        {
            remain-=writen;
            
        }else
        {
            int savedErrno=errno;
            if(savedErrno==EPIPE)
            {
                Logmanage(ERR,"tcpconntion::sendloop peer close");
                handleClose();
                return;
            }
            else if(savedErrno==EWOULDBLOCK||savedErrno==EAGAIN)
            {
                writen=0;
            }else
            {
                errno=savedErrno;
                Logmanage(ERR,"tcpconntion::sendloop write fail:%s",strerror(errno));
				handleClose();
                return;
            }
        }

    }
    if(remain>0)
   	{
        outbuffer_.append(data+(len-remain),remain);
       	if(!connchannel_->isWriting())
        {
            connchannel_->SetEnableOut();
        }
    }
    
}

void TcpConnction::handleWrite()
{
	while(connchannel_->isWriting()&&state_==kConnected)
    {
        int savedErrno=0;
        ssize_t n=outbuffer_.writeFd(connsock_->fd(),&savedErrno);
        if(n>=0)
        {
			outbuffer_.retrieve(n);
            if(outbuffer_.readableBytes()==0)
            {
                connchannel_->SetDesableOut();
                break;
            }
            // if(state_!=kConnected)
            // {
            //     shutdownInloop();
            // }
        }
        else
        {
            if(savedErrno==EAGAIN||savedErrno==EWOULDBLOCK||savedErrno==EINTR)
            {
                continue;
            }
            else if(savedErrno==EPIPE)
            {
                handleClose();
                break;
            }
            else
            {
                errno=savedErrno;
				Logmanage(ERR,"tcpconntion::handlewrite fali:%d->%s",errno,strerror(errno));
            	handleClose();
                break;
            }
        }
    }
}
// void TcpConnction::shutdown()
// {
// 	if(state_==kConnected)
//     {
//         state_=kDisConnecting;
// 		loop_->runInLoop(std::bind(&TcpConnction::shutdownInloop,shared_from_this()));
//     }
// }
// void TcpConnction::shutdownInloop()
// {
// 	state_=kDisconnected;
//     connchannel_->remove();
// 	conntionCallback_(shared_from_this());
//     closeCallback_(shared_from_this());
// }
void TcpConnction::handleClose()
{
    state_=kDisConnecting;
    connchannel_->Setnonable();
    Logmanage(NORMOL,"conntion fd:%d,break",connsock_->fd());
    conntionCallback_(shared_from_this());
    closeCallback_(shared_from_this());
}
void TcpConnction::handleErr()
{
	int optval,err;
    socklen_t len=sizeof(optval);
    if(getsockopt(connsock_->fd(),SOL_SOCKET,SO_ERROR,&optval,&len)<0)
    {
        err=errno;
    }
    else
    {
        err=optval;
    }
    Logmanage(ERR,"tcpsonntion::handleErr:%d->%s", err,strerror(err));
}