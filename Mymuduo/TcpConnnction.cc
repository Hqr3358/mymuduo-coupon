#include"TcpConnnction.hpp"
#include"Channel.hpp"
#include"Socket.hpp"
#include "EventLoop.hpp"
#include"log.hpp"
#include"TimeStamp.hpp"
#include"BusinessThreadPool.hpp"
#include"RedisClient.hpp"
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
,businessPool_(nullptr)
,redisClient_(nullptr)
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
    // 连接建立时写入 Redis ZSET，score=当前时间
    if (redisClient_)
    {
        redisClient_->updateActiveTime(name_, TimeStamp::now());
    }
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
    while(true)
    {
		ssize_t n=inbuffer_.readFd(connsock_->fd(),&savedErrno);
    	if(n>0)
    	{
            // IO 线程已经把数据读完，拷贝出独立 Buffer 交给业务线程池处理，
            // 避免业务逻辑阻塞 IO 线程，同时避免 inbuffer_ 被并发读写。
            auto spBuf = std::make_shared<Buffer>();
            spBuf->append(inbuffer_.peek(), inbuffer_.readableBytes());
            inbuffer_.retrieveAll();

            // 更新 Redis 中该连接的活跃时间
            if (redisClient_)
            {
                redisClient_->updateActiveTime(name_, TimeStamp::now());
            }

            // 投递到业务线程池异步执行消息回调
            if (businessPool_)
            {
                TcpConnctionptr self = shared_from_this();
                // enqueue返回false代表队列满，限流拒绝
                bool ok = businessPool_->enqueue([self, spBuf, ti]()
                {
                    self->messageCallback_(self, spBuf.get(), ti);
                });
                if (!ok)
                {
                    // 队列满，业务线程处理不过来，这条消息直接丢弃并打日志
                    Logmanage(ERR, "business pool full, drop msg from conn:%s", name_.c_str());
                }
            }
            else
            {
                messageCallback_(shared_from_this(), spBuf.get(), ti);
            }
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
            // 注意：这里把 msg 拷贝成独立 string 捕获，避免悬垂指针
            /**
             * @brief 新增：修复悬垂指针（原实现 std::bind 只捕获 spMsg->data() 裸指针、
             *        未持有 shared_ptr，spMsg 析构后指针失效，IO 线程 sendloop 读到已复用
             *        内存，实测响应头出现垃圾字节 0x02801dff）。改为 lambda 捕获 shared_ptr，
             *        保证字符串与连接生命周期延续到发送完成。
             */
            auto spMsg = std::make_shared<std::string>(msg);
            loop_->queueInLoop([self = shared_from_this(), spMsg]()
            {
                self->sendloop(spMsg->data(), static_cast<int>(spMsg->size()));
            });
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
void TcpConnction::handleClose()
{
    state_=kDisConnecting;
    connchannel_->Setnonable();
    // 从 Redis ZSET 中移除该连接
    if (redisClient_)
    {
        redisClient_->removeConn(name_);
    }
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
