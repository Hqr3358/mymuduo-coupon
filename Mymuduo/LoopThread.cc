#include"LoopThread.hpp"
#include"CurrentThread.hpp"
#include <memory>
#include<semaphore.h>
Thread::Thread(Func fun):
tid_(0)
,thread_(nullptr)
,started_(false)
,joined_(false)
,func_(std::move(fun))
{}
Thread::~Thread()
{}
void Thread::start()
{
    started_=true;
    sem_t sem;
    sem_init(&sem,false,0);
    thread_=std::make_unique<std::thread>([&]()
	{
    	tid_=CurrentThread::tid();
        sem_post(&sem);
    	func_();
	});
	sem_wait(&sem);
}