#pragma once
#include "paper_r7_unified_input.hpp"
#include <cerrno>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <sys/mman.h>
#include <unistd.h>

namespace zkgpt::r7::oneblock::unified::detail {
// One root owns one anonymous mapping. No K/U state or allocator is shared.
class KFrBuffer {
 Fr* data_=nullptr;
 std::size_t size_=0,mapped_bytes_=0,page_size_=0;
 std::size_t aligned(std::size_t count)const{
  if(count>std::numeric_limits<std::size_t>::max()/sizeof(Fr))
   throw std::runtime_error("K mmap allocation size overflow: elements="+std::to_string(count));
  auto bytes=count*sizeof(Fr);
  if(bytes>std::numeric_limits<std::size_t>::max()-(page_size_-1))
   throw std::runtime_error("K mmap page rounding overflow: requested_bytes="+std::to_string(bytes));
  return ((bytes+page_size_-1)/page_size_)*page_size_;
 }
 public:
 KFrBuffer()=default;
 explicit KFrBuffer(std::size_t count){
  static_assert(std::is_nothrow_destructible<Fr>::value,"K Fr destructor must not throw");
  auto page=::sysconf(_SC_PAGESIZE);
  if(page<=0 || std::size_t(page)%alignof(Fr)!=0)throw std::runtime_error("K mmap page size/alignment unavailable");
  page_size_=std::size_t(page);auto bytes=aligned(count);
  if(!bytes)return;
  auto ptr=::mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  if(ptr==MAP_FAILED){
   auto error=errno;
   throw std::runtime_error("K mmap allocation failed: requested_bytes="+std::to_string(bytes)+
                            " elements="+std::to_string(count)+" errno="+std::to_string(error));
  }
  data_=static_cast<Fr*>(ptr);mapped_bytes_=bytes;
  try{for(;size_<count;++size_)::new(static_cast<void*>(data_+size_))Fr();}
  catch(...){clear();throw;}
 }
 ~KFrBuffer(){clear();}
 KFrBuffer(const KFrBuffer&)=delete;KFrBuffer&operator=(const KFrBuffer&)=delete;
 KFrBuffer(KFrBuffer&&other)noexcept{swap(other);}
 KFrBuffer&operator=(KFrBuffer&&other)noexcept{
  if(this!=&other){clear();swap(other);}return *this;
 }
 void swap(KFrBuffer&other)noexcept{
  std::swap(data_,other.data_);std::swap(size_,other.size_);
  std::swap(mapped_bytes_,other.mapped_bytes_);std::swap(page_size_,other.page_size_);
 }
 Fr*data()noexcept{return data_;}const Fr*data()const noexcept{return data_;}
 std::size_t size()const noexcept{return size_;}
 std::size_t mappedBytes()const noexcept{return mapped_bytes_;}
 Fr&operator[](std::size_t i)noexcept{return data_[i];}
 const Fr&operator[](std::size_t i)const noexcept{return data_[i];}
 void shrinkTo(std::size_t count){
  if(count>size_)throw std::logic_error("K mmap buffer cannot grow");
  if(count==size_)return;
  const auto bytes=aligned(count);
  std::destroy_n(data_+count,size_-count);size_=count;
  if(bytes<mapped_bytes_){
   // Keep the old mapping extent if munmap fails, so clear() can release it.
   if(::munmap(reinterpret_cast<char*>(data_)+bytes,mapped_bytes_-bytes)!=0)
    throw std::runtime_error("K mmap tail munmap failed: requested_bytes="+
                             std::to_string(mapped_bytes_-bytes)+" errno="+std::to_string(errno));
   mapped_bytes_=bytes;if(!bytes)data_=nullptr;
  }
 }
 void clear()noexcept{
  if(data_){
   std::destroy_n(data_,size_);size_=0;
   // Match FoldedU's destructor policy, including one transient-error retry.
   if(::munmap(data_,mapped_bytes_)!=0 && ::munmap(data_,mapped_bytes_)!=0)std::terminate();
  }
  data_=nullptr;size_=mapped_bytes_=page_size_=0;
 }
};
}
