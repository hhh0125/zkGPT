#pragma once
#include "classified_layout.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <unistd.h>
namespace zkgpt::r7::oneblock::classified::mapping {
// Public, fully observed coordinates only. Logical step preserves multiplicity
// when one root is interleaved with other roots or constant locations.
struct Run {std::size_t begin,count;Coordinate first;std::int64_t stride=0;std::size_t logical_step=1;};
inline constexpr std::uint64_t absent=std::numeric_limits<std::uint64_t>::max();
inline constexpr std::size_t block_runs=4096;
struct RootSpan {std::uint64_t head=absent,tail=absent,runs=0;};
struct Block {std::uint64_t next=absent,count=0;};
class Store {
 int fd_=-1;
 void write(const void*data,std::size_t n,std::uint64_t at){
  auto*p=static_cast<const unsigned char*>(data);while(n){auto done=::pwrite(fd_,p,n,off_t(at));if(done<0 && errno==EINTR)continue;need(done>0,"classified mapping plan write failed");p+=done;n-=std::size_t(done);at+=std::size_t(done);}
 }
 void read(void*data,std::size_t n,std::uint64_t at)const{
  need(at<=bytes && n<=bytes-at,"classified mapping plan read bounds");auto*p=static_cast<unsigned char*>(data);while(n){auto done=::pread(fd_,p,n,off_t(at));if(done<0 && errno==EINTR)continue;need(done>0,"classified mapping plan read failed");p+=done;n-=std::size_t(done);at+=std::size_t(done);}
 }
 public:
 std::uint64_t limit,bytes=0;mutable std::uint64_t read_bytes=0,read_blocks=0;
 explicit Store(std::uint64_t cap):limit(cap){}
 Store(const Store&)=delete;Store&operator=(const Store&)=delete;
 ~Store(){if(fd_>=0)::close(fd_);}
 bool append(RootSpan&span,const Run*runs,std::size_t n){
  need(n && n<=block_runs,"classified mapping plan block count");auto size=sizeof(Block)+n*sizeof(Run);if(bytes>limit || size>limit-bytes)return false;
  // Anonymous inode belongs to the fixed execution directory and this one
  // authentication session; no historical file or shared cache is opened.
  if(fd_<0){fd_=::open(".",O_TMPFILE|O_RDWR|O_CLOEXEC,0600);need(fd_>=0,"classified anonymous mapping plan open failed");}
  auto offset=bytes;Block header{absent,n};write(&header,sizeof(header),offset);write(runs,n*sizeof(Run),offset+sizeof(header));
  if(span.tail!=absent)write(&offset,sizeof(offset),span.tail);else span.head=offset;
  span.tail=offset;span.runs+=n;bytes+=size;return true;
 }
 template<class F>void each(const RootSpan&span,F&&consume)const{
  if(span.head==absent){need(span.runs==0,"classified empty mapping plan");return;}
  std::array<Run,block_runs>buffer;auto at=span.head;std::uint64_t seen=0;
  while(at!=absent){Block h;read(&h,sizeof(h),at);need(h.count && h.count<=block_runs && h.count<=span.runs-seen,"classified mapping plan chain count");
   read(buffer.data(),h.count*sizeof(Run),at+sizeof(h));read_bytes+=sizeof(h)+h.count*sizeof(Run);++read_blocks;
   for(std::size_t i=0;i<h.count;++i){consume(buffer[i]);}
   seen+=h.count;need(h.next==absent || h.next>at,"classified mapping plan chain order");at=h.next;
  }need(seen==span.runs,"classified mapping plan chain incomplete");
 }
};
struct Plan {
 std::vector<Run>runs;std::vector<RootSpan>disk_roots;bool frozen=false;const char*reason="UNPREPARED";std::size_t run_count=0;
 std::size_t bytes()const{return runs.capacity()*sizeof(Run)+disk_roots.capacity()*sizeof(RootSpan);}
 bool hasRoot(std::size_t k)const{if(!disk_roots.empty())return disk_roots.at(k).runs!=0;auto first=std::lower_bound(runs.begin(),runs.end(),k,[](const Run&r,std::size_t root){return r.first.root<root;});return first!=runs.end() && first->first.root==k;}
 template<class F>void forRoot(std::size_t k,const Store&store,F&&consume)const{
  if(!disk_roots.empty()){store.each(disk_roots.at(k),consume);return;}
  auto first=std::lower_bound(runs.begin(),runs.end(),k,[](const Run&r,std::size_t root){return r.first.root<root;});for(;first!=runs.end() && first->first.root==k;++first)consume(*first);
 }
};
// Construction remains bounded even for arbitrary fragmented callbacks. Only
// the complete canonical digest check in prepareClaim publishes frozen=true.
class Builder {
 Plan&plan_;Store&store_;const std::size_t available_,roots_,maximum_;std::vector<std::size_t>last_;bool fits_;
 void discard(const char*why){fits_=false;plan_.reason=why;std::vector<Run>().swap(plan_.runs);std::vector<RootSpan>().swap(plan_.disk_roots);}
 bool flush(){
  if(plan_.disk_roots.empty())plan_.disk_roots.resize(roots_);
  std::sort(plan_.runs.begin(),plan_.runs.end(),[](const Run&a,const Run&b){return a.first.root<b.first.root || (a.first.root==b.first.root && a.begin<b.begin);});
  for(std::size_t begin=0;begin<plan_.runs.size();){auto end=begin+1;auto k=plan_.runs[begin].first.root;while(end<plan_.runs.size() && plan_.runs[end].first.root==k)++end;
   if(!store_.append(plan_.disk_roots.at(k),plan_.runs.data()+begin,end-begin)){discard("FILE_BUDGET_EXHAUSTED");return false;}begin=end;
  }plan_.runs.clear();std::fill(last_.begin(),last_.end(),std::size_t(-1));return true;
 }
 public:
 Builder(Plan&p,Store&s,std::size_t available,std::size_t roots):plan_(p),store_(s),available_(available),roots_(roots),maximum_(available>roots*(sizeof(RootSpan)+sizeof(std::size_t))?std::min(block_runs,(available-roots*(sizeof(RootSpan)+sizeof(std::size_t)))/sizeof(Run)):0),last_(maximum_?roots:0,std::size_t(-1)),fits_(maximum_>0){if(!fits_)plan_.reason="RAM_METADATA_BUDGET_EXHAUSTED";}
 void append(std::size_t j,Coordinate coordinate){
  if(!fits_){return;}
  auto k=coordinate.root;need(k<roots_,"classified mapping plan root bounds");auto index=last_[k];
  if(index!=std::size_t(-1)){auto&r=plan_.runs[index];auto logical=j-r.begin;auto delta=std::int64_t(coordinate.index)-r.first.index;
   if(r.count==1){r.logical_step=logical;r.stride=delta;++r.count;return;}
   if(logical==r.count*r.logical_step && delta==std::int64_t(r.count)*r.stride){++r.count;return;}
  }
  if(plan_.runs.size()==maximum_ && !flush())return;
  if(plan_.runs.size()==plan_.runs.capacity())plan_.runs.reserve(std::min(maximum_,std::max<std::size_t>(8,plan_.runs.capacity()*2)));
  last_[k]=plan_.runs.size();plan_.runs.push_back({j,1,coordinate});++plan_.run_count;
 }
 void finish(){
  if(!fits_)return;
  if(!plan_.disk_roots.empty()){if(!plan_.runs.empty() && !flush())return;std::vector<Run>().swap(plan_.runs);plan_.reason="FROZEN_ROOT_FILE";}
  else{std::sort(plan_.runs.begin(),plan_.runs.end(),[](const Run&a,const Run&b){return a.first.root<b.first.root || (a.first.root==b.first.root && a.begin<b.begin);});plan_.reason="FROZEN_ROOT_RAM";}
  need(plan_.bytes()<=available_,"classified mapping plan RAM budget");plan_.frozen=true;
 }
};
}
