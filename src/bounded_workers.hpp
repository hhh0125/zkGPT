#pragma once
#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <mutex>
#include <pthread.h>
#include <stdexcept>

namespace zkgpt {
inline constexpr unsigned maxWorkers = 32;
// Synchronous pool shared by a single authentication's CPU phases.
// Only the owner dispatches. Jobs complete before any borrowed storage is released.
class Workers {
 unsigned count_;std::array<pthread_t,maxWorkers-1>threads_{};unsigned created_=0;std::mutex mutex_;std::array<std::condition_variable,maxWorkers-1>wake_;std::condition_variable done_;
 struct Context {Workers*self;unsigned id;};std::array<Context,maxWorkers-1>contexts_{};
 bool stop_=false;std::size_t generation_=0,pending_=0,tasks_=0,participants_=0;const void*job_context_=nullptr;void(*job_run_)(const void*,std::size_t)=nullptr;
 std::array<std::exception_ptr,maxWorkers>errors_{};
 void work(unsigned id){try{for(std::size_t j=id;j<tasks_;j+=count_)job_run_(job_context_,j);}catch(...){errors_[id]=std::current_exception();}}
 public:static constexpr unsigned capacity = maxWorkers;
 explicit Workers(unsigned count):count_(count){
  if(count_<1 || count_>maxWorkers)throw std::runtime_error("worker count outside 1..32");
  if(count_==1)return;
  pthread_attr_t attr;if(pthread_attr_init(&attr)!=0)throw std::runtime_error("worker thread attributes");
  if(pthread_attr_setstacksize(&attr,1u<<20)!=0){pthread_attr_destroy(&attr);throw std::runtime_error("worker bounded thread stack");}
  try{for(unsigned id=1;id<count_;++id){contexts_[id-1]={this,id};auto error=pthread_create(&threads_[id-1],&attr,[](void*arg)->void*{auto context=*static_cast<Context*>(arg);context.self->loop(context.id);return nullptr;},&contexts_[id-1]);if(error!=0)throw std::runtime_error("worker creation");++created_;}pthread_attr_destroy(&attr);}
  catch(...){pthread_attr_destroy(&attr);shutdown();throw;}
 }
 void loop(unsigned id){std::size_t seen=0;for(;;){std::unique_lock<std::mutex>lock(mutex_);wake_[id-1].wait(lock,[&]{return stop_ || (generation_!=seen && id<participants_);});if(stop_)return;seen=generation_;lock.unlock();work(id);lock.lock();if(--pending_==0)done_.notify_one();}}
 void shutdown(){ {std::lock_guard<std::mutex>lock(mutex_);stop_=true;}for(unsigned id=0;id<created_;++id)wake_[id].notify_one();for(unsigned id=0;id<created_;++id)pthread_join(threads_[id],nullptr);created_=0;}
 ~Workers(){shutdown();}
 unsigned count()const{return count_;}
 template<class F>void run(std::size_t count,const F&fn,bool parallel){
  if(!parallel || count_==1 || count<=1){for(std::size_t j=0;j<count;++j)fn(j);return;}
  // Borrow the caller's live closure until this synchronous join completes.
  // No std::function heap allocation/copy for each tile dispatch.
  {std::lock_guard<std::mutex>lock(mutex_);job_context_=&fn;job_run_=[](const void*p,std::size_t j){(*static_cast<const F*>(p))(j);};tasks_=count;participants_=std::min<std::size_t>(count,count_);pending_=participants_-1;errors_={};++generation_;}
  for(unsigned id=1;id<participants_;++id)wake_[id-1].notify_one();
  work(0);
  {std::unique_lock<std::mutex>lock(mutex_);done_.wait(lock,[&]{return pending_==0;});job_context_=nullptr;job_run_=nullptr;}
  for(auto&e:errors_)if(e)std::rethrow_exception(e);
 }
};
}
