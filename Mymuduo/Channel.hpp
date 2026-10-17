#pragma once
#include <cstdint>
#include<functional>
#include<memory>
#include<sys/epoll.h>
#include <sys/types.h>

class EventLoop;
class TimeStamp;
//Channel类和epoll直接接触，接收epoll返回就绪事件和对epoll进行更新ctl
class Channel
{
    using EventCallback=std::function<void()>;
    using racvCallback=std::function<void(TimeStamp&)>;
public:
	explicit Channel(EventLoop* loop,int fd):
    tied_(false)
    ,loop_(loop)
    ,index_(-1)
    ,fd_(fd)
    ,events_(0)
    ,revents_(0){}
    ~Channel(){}
	//调用epoll返回的就绪事件接口
	void FunHadler(int fd);
	//设置对应回调函数
	void SetrecvCallback(racvCallback&& cb)
    {
        recvCallback_=std::move(cb);
    }
    void SetWriteCallback(EventCallback&& cb)
    {
        WriteCallback_=std::move(cb);
    }
    void SetCloseCallback(EventCallback&& cb)
    {
        CloseCallback_=std::move(cb);
    }
    void SetErrCallback(EventCallback&& cb)
    {
        ErrCallback_=std::move(cb);
    }
   	//交给上层eventloop让他通知epoll更新
    void update(); 
    //对event赋值并给相应的epoll全是ET模式
    void SetEnableIn()
    {
        events_|=EPOLLIN|EPOLLET;
        update();
    }
    void SetDesableIn()
    {
        events_&=~EPOLLIN;
        update();
    }
    void SetEnableOut()
    {
        events_|=EPOLLOUT|EPOLLET;
        update();
    }
    void SetDesableOut()
    {
        events_&=~EPOLLOUT;
        update();
    }
    void Setnonable()
    {
        events_=0;
        update();
    }
    //设置index
    void Setindex(int index)
    {
        index_=index;
    }
    //该channel不可用了调用上层eventloop在epoll监听中删除
    void remove();
    //epoll返回就绪事件并设置
    void SetRevent(uint32_t revent)
    {
		revents_=revent;
    }
public:
	bool isWriting()
    {
        return events_&EPOLLOUT;
    }
	int fd(){return fd_;}
    uint32_t events(){return events_;}
	int index(){return index_;}
    void tie(const std::shared_ptr<void>& cb);
private:
	void HandleEvent(int fd);//调用对应回调函数
	std::weak_ptr<void> tie_;
    bool tied_;
	EventLoop* loop_;
    int index_;//在epoll中的状态
    int fd_;//该channel管理的那个fd
	uint32_t events_;//该fd需要那些事件被监听
    uint32_t revents_;//接收epoll返回的就绪事件
    //事件就绪对应的回调函数
    racvCallback recvCallback_;
    EventCallback WriteCallback_;
    EventCallback CloseCallback_;
    EventCallback ErrCallback_;
};