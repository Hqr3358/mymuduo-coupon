#pragma once
#include <cstdint>
#include<functional>
#include<memory>
#include<sys/epoll.h>
#include <sys/types.h>

class EventLoop;
class TimeStamp;
//Channel类和epoll直接接触，接收epoll返回就绪事件和对epoll进行更新ctl
/** @brief Channel：fd 与监听事件的绑定，接收 epoll 就绪事件并分发回调 */
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
/** @brief 事件分发：按 revents 调用读/写/关闭/错误回调 */
	void FunHadler(int fd);
	//设置对应回调函数
/** @brief 设置可读事件回调 */
	void SetrecvCallback(racvCallback&& cb)
    {
        recvCallback_=std::move(cb);
    }
/** @brief 设置可写事件回调 */
    void SetWriteCallback(EventCallback&& cb)
    {
        WriteCallback_=std::move(cb);
    }
/** @brief 设置关闭事件回调 */
    void SetCloseCallback(EventCallback&& cb)
    {
        CloseCallback_=std::move(cb);
    }
/** @brief 设置错误事件回调 */
    void SetErrCallback(EventCallback&& cb)
    {
        ErrCallback_=std::move(cb);
    }
   	//交给上层eventloop让他通知epoll更新
/** @brief 更新本 Channel 到事件循环（注册/修改监听事件） */
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
/** @brief 从事件循环移除本 Channel */
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
/** @brief 绑定对象弱引用（延长生命周期，防止回调时对象已析构） */
    void tie(const std::shared_ptr<void>& cb);
private:
/** @brief 事件总入口：持弱引用锁保证对象存活后分发 */
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