#include "public_k.hpp"
#include "opt2_metrics.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <openssl/sha.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
namespace zkgpt::r7::oneblock::unified {
namespace {
using Clock=std::chrono::steady_clock;
constexpr std::size_t chunk=4096;
constexpr std::uint64_t dirtyWindow=16ull<<20;
double seconds(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
Digest hash(const Fr*p,std::size_t n){Digest d{};need(SHA256(reinterpret_cast<const unsigned char*>(p),n*sizeof(Fr),d.data()),"public K checksum failure");return d;}
std::uint64_t limit(const char*name,std::uint64_t value,std::uint64_t maximum){auto*s=std::getenv(name);if(!s)return value;std::string text(s);need(!text.empty() && text.find_first_not_of("0123456789")==std::string::npos,"public K invalid budget");std::size_t used=0;auto n=std::stoull(text,&used);need(used==text.size() && n<=maximum,"public K budget cap");return n;}
// Consider every applicable cgroup ancestor, including memory charged for file
// cache. Missing/unreadable controls disable the file optimization, not safety.
bool memoryRoom(std::uint64_t reserve){
 std::ifstream cg("/proc/self/cgroup");std::string line,path;while(std::getline(cg,line))if(line.rfind("0::/",0)==0)path=line.substr(3);
 if(path.empty())return false;
 std::string directory="/sys/fs/cgroup"+path;bool inspected=false;
 for(;;){std::ifstream cap(directory+"/memory.max"),usage(directory+"/memory.current");std::string maximum;std::uint64_t current=0;
  if(cap>>maximum){inspected=true;if(maximum!="max"){if(!(usage>>current))return false;auto bound=std::stoull(maximum);if(current>=bound || reserve>bound-current)return false;}}
  if(directory=="/sys/fs/cgroup")break;
  auto pos=directory.find_last_of('/');if(pos<std::string("/sys/fs/cgroup").size())directory="/sys/fs/cgroup";else directory.resize(pos);
 }
 std::ifstream info("/proc/meminfo");std::string key,unit;std::uint64_t kb=0,available=0;while(info>>key>>kb>>unit)if(key=="MemAvailable:")available=kb*1024;
 std::ifstream pressure("/proc/pressure/memory");std::string kind,average;if(!(pressure>>kind>>average) || average.rfind("avg10=",0)!=0)return false;
 return inspected && available>reserve && std::stod(average.substr(6))<1.0;
}
struct File {
 int fd=-1;std::size_t count=0;std::vector<Digest>checksums;KMetrics&m;
 explicit File(KMetrics&metrics):m(metrics){}
 ~File(){if(fd>=0)::close(fd);}
 void transfer(void*data,std::size_t bytes,std::uint64_t at,bool writing){
  need(at<=count*sizeof(Fr) && bytes<=count*sizeof(Fr)-at,"public K file bounds");auto started=Clock::now();auto*p=static_cast<unsigned char*>(data);
  while(bytes){auto n=writing ? ::pwrite(fd,p,bytes,off_t(at)) : ::pread(fd,p,bytes,off_t(at));if(n<0 && errno==EINTR)continue;need(n>0,"public K file I/O failed or premature EOF");p+=n;bytes-=std::size_t(n);at+=std::size_t(n);if(writing)m.file_write_bytes+=n;else m.file_read_bytes+=n;}
  m.io_wall_seconds+=seconds(started);
 }
 void read(std::size_t page,Fr*out){auto start=page*chunk;need(start<count,"public K page bounds");auto n=std::min(chunk,count-start);transfer(out,n*sizeof(Fr),start*sizeof(Fr),false);need(hash(out,n)==checksums.at(page),"public K corrupted file state");auto started=Clock::now();need(::posix_fadvise(fd,off_t(start*sizeof(Fr)),off_t(n*sizeof(Fr)),POSIX_FADV_DONTNEED)==0,"public K read page cache advice failed");m.io_wall_seconds+=seconds(started);}
 void write(std::size_t page,Fr*in,std::size_t n){need(n && n<=chunk,"public K write page bound");transfer(in,n*sizeof(Fr),page*chunk*sizeof(Fr),true);checksums.at(page)=hash(in,n);}
 void evict(){auto started=Clock::now();int rc;do{rc=::fdatasync(fd);}while(rc<0 && errno==EINTR);need(rc==0,"public K file sync failed");need(::posix_fadvise(fd,0,0,POSIX_FADV_DONTNEED)==0,"public K page cache advice failed");m.io_wall_seconds+=seconds(started);}
};
}
KBudget KBudget::environment(){return {limit("ZKGPT_K_RAM_BYTES",512ull<<20,2ull<<30),limit("ZKGPT_K_FILE_BYTES",4ull<<30,8ull<<30)};}
struct PublicK::Impl {
 struct Root {std::vector<Fr>ram;std::unique_ptr<File>file;std::size_t size=0,builds=0;bool touched=false;};
 std::vector<Segment>segments;Build build;KBudget budget;std::string session;std::vector<Fr>point;std::vector<Root>roots;unsigned bits=0;bool ended=false,failed=false,compact=true;
 KMetrics metrics;std::uint64_t ram_bytes=0,file_bytes=0;bool files_allowed=false;
 // One scratch window shared by all roots; public K only. File checksum
 // metadata is separately bounded by file budget / chunk size.
 std::array<Fr,2*chunk>scratch;std::size_t cached_root=std::size_t(-1),cached_page=0;
 Impl(const std::vector<Segment>&s,Build b,KBudget cap,const std::string&id,std::size_t domain):segments(s),build(std::move(b)),budget(cap),session(id),roots(s.size()){
  need(build && !session.empty() && !segments.empty() && segments.size()<=8193 && budget.ram<=2ull<<30 && budget.file<=8ull<<30,"public K constructor bound");
  std::size_t end=0;for(auto segment:segments){need(segment.padded && !(segment.padded&(segment.padded-1)) && segment.padded<=r1::kMaxVector && segment.offset<=(std::size_t(1)<<32)-segment.padded,"public K physical root bound");end=std::max(end,segment.offset+segment.padded);}
  if(!domain){domain=1;while(domain<end)domain*=2;}validate(domain,segments);while(domain>1){++bits;domain/=2;}
  struct statvfs fs{};files_allowed=budget.file && ::statvfs(".",&fs)==0 && fs.f_bavail*std::uint64_t(fs.f_frsize)>budget.file+(1ull<<30) && memoryRoom(budget.ram+dirtyWindow+(64ull<<20));
  if(const auto*v=std::getenv("ZKGPT_K_COMPACT")){std::string text(v);need(text=="0" || text=="1","K compaction switch");compact=text=="1";}
  peak();
 }
 void check(const std::string&id)const{need(id==session,"public K foreign authentication session");}
 void peak(std::uint64_t temporary=0){std::uint64_t metadata=roots.capacity()*sizeof(Root)+segments.capacity()*sizeof(Segment)+point.capacity()*sizeof(Fr)+session.capacity();for(auto&r:roots)if(r.file)metadata+=sizeof(File)+r.file->checksums.capacity()*sizeof(Digest);metrics.ram_peak_bytes=std::max(metrics.ram_peak_bytes,ram_bytes+temporary+sizeof(scratch)+metadata);metrics.file_peak_bytes=std::max(metrics.file_peak_bytes,file_bytes);}
 Fr at(std::size_t k,std::size_t j){auto&r=roots.at(k);need(j<r.size,"public K read index");++metrics.state_read_coordinates;if(!r.file)return r.ram.at(j);auto page=j/chunk;if(cached_root!=k || cached_page!=page){r.file->read(page,scratch.data());cached_root=k;cached_page=page;}return scratch[j%chunk];}
 void with(std::size_t k,const std::function<void(const Value&)>&consume){
  auto&r=roots.at(k);r.touched=true;if(!r.ram.empty() || r.file){consume([this,k](std::size_t j){return at(k,j);});return;}
  auto started=Clock::now();auto v=build(k);need(v.size()==segments[k].padded,"public K builder shape");metrics.builder_peak_bytes=std::max<std::uint64_t>(metrics.builder_peak_bytes,v.capacity()*sizeof(Fr));peak(v.capacity()*sizeof(Fr));++metrics.initial_builds;if(r.builds++)++metrics.fallback_rebuild_calls;
  metrics.initial_build_seconds+=seconds(started);std::size_t count=v.size();auto cpu=crypto::processCPU();auto foldStarted=Clock::now();
  for(std::size_t t=0;t<point.size() && count>1;++t){for(std::size_t j=0;j<count/2;++j)v[j]=v[2*j]+point[t]*(v[2*j+1]-v[2*j]);metrics.fold_coordinates+=count;count/=2;}
  metrics.fold_cpu_seconds+=crypto::processCPU()-cpu;metrics.fold_wall_seconds+=seconds(foldStarted);r.size=count;
  auto bytes=count*sizeof(Fr);
  if(bytes<=budget.ram-ram_bytes){
   if(v.capacity()*sizeof(Fr)<=budget.ram-ram_bytes){v.resize(count);r.ram=std::move(v);}else r.ram.assign(v.begin(),v.begin()+count);
   ram_bytes+=r.ram.capacity()*sizeof(Fr);++metrics.ram_roots;peak(v.capacity()*sizeof(Fr));consume([this,k](std::size_t j){return at(k,j);});return;
  }
  if(files_allowed && bytes<=budget.file-file_bytes && memoryRoom(v.capacity()*sizeof(Fr)+budget.ram+dirtyWindow+(64ull<<20))){
   auto f=std::make_unique<File>(metrics);f->fd=::open(".",O_TMPFILE|O_RDWR|O_CLOEXEC,0600);
   if(f->fd>=0){struct stat info{};need(::fstat(f->fd,&info)==0 && (info.st_mode&0777)==0600,"public K file permissions");f->count=count;need(::ftruncate(f->fd,off_t(bytes))==0,"public K file allocation failed");f->checksums.resize((count+chunk-1)/chunk);
    for(std::size_t start=0;start<count;start+=chunk){f->write(start/chunk,v.data()+start,std::min(chunk,count-start));if(((start+chunk)*sizeof(Fr))%dirtyWindow==0)f->evict();}
    f->evict();r.file=std::move(f);file_bytes+=bytes;++metrics.file_roots;peak(v.capacity()*sizeof(Fr));std::vector<Fr>().swap(v);cached_root=std::size_t(-1);consume([this,k](std::size_t j){return at(k,j);});return;
   }files_allowed=false;
  }
  ++metrics.rebuild_roots;consume([&](std::size_t j){need(j<count,"public K fallback bounds");++metrics.state_read_coordinates;return v[j];});
 }
 void advance(const Fr&r){
  auto started=Clock::now();auto cpu=crypto::processCPU();cached_root=std::size_t(-1);
  for(auto&root:roots){need(root.touched,"public K incomplete round state");root.touched=false;if(root.size<=1 || (root.ram.empty() && !root.file))continue;
   auto old=root.size;metrics.fold_coordinates+=old;
   if(!root.file){for(std::size_t j=0;j<old/2;++j)root.ram[j]=root.ram[2*j]+r*(root.ram[2*j+1]-root.ram[2*j]);root.ram.resize(old/2);}
   else{auto&f=*root.file;
    // Consume both input pages before writing a lower output page. Neither
    // unread input values nor unread checksums are overwritten.
    for(std::size_t start=0;start<old;start+=2*chunk){auto count=std::min(2*chunk,old-start);f.read(start/chunk,scratch.data());if(count>chunk)f.read(start/chunk+1,scratch.data()+chunk);for(std::size_t j=0;j<count/2;++j)scratch[j]=scratch[2*j]+r*(scratch[2*j+1]-scratch[2*j]);f.write(start/(2*chunk),scratch.data(),count/2);if(((start+2*chunk)/2*sizeof(Fr))%dirtyWindow==0)f.evict();}
    f.count=old/2;f.checksums.resize((f.count+chunk-1)/chunk);need(::ftruncate(f.fd,off_t(f.count*sizeof(Fr)))==0,"public K truncate folded state");file_bytes-=(old/2)*sizeof(Fr);f.evict();
   }root.size=old/2;
   if(compact && !root.file && root.ram.capacity()>root.ram.size()){
    auto begin=Clock::now();auto old_bytes=root.ram.capacity()*sizeof(Fr);std::vector<Fr>smaller(root.ram.begin(),root.ram.end());
    auto new_bytes=smaller.capacity()*sizeof(Fr);need(new_bytes<=old_bytes,"K compaction capacity grew");peak(new_bytes);
    root.ram.swap(smaller);ram_bytes-=old_bytes-new_bytes;++metrics.ram_compactions;metrics.ram_released_bytes+=old_bytes-new_bytes;metrics.compaction_copy_bytes+=new_bytes;metrics.compaction_seconds+=seconds(begin);
   }
  }point.push_back(r);metrics.fold_cpu_seconds+=crypto::processCPU()-cpu;metrics.fold_wall_seconds+=seconds(started);peak();
 }
};
PublicK::PublicK(const std::vector<Segment>&s,Build b,KBudget cap,const std::string&id,std::size_t domain):p_(std::make_unique<Impl>(s,std::move(b),cap,id,domain)){}
PublicK::~PublicK()=default;
void PublicK::withRoot(std::size_t k,const std::string&id,const std::function<void(const Value&)>&consume){p_->check(id);need(!p_->ended && !p_->failed && bool(consume),"public K closed/failed state or missing consumer");try{p_->with(k,consume);}catch(...){p_->failed=true;throw;}}
void PublicK::advance(const std::string&id,const Fr&r){p_->check(id);need(!p_->ended && !p_->failed && r.isValid() && p_->point.size()<p_->bits,"public K challenge/phase bound");try{p_->advance(r);}catch(...){p_->failed=true;throw;}}
Fr PublicK::endpoint(const std::string&id,const std::vector<Fr>&point){p_->check(id);need(!p_->ended && !p_->failed && point==p_->point && point.size()==p_->bits,"public K endpoint state order/terminal dimension");p_->ended=true;Fr out=0;for(std::size_t k=0;k<p_->segments.size();++k)p_->with(k,[&](const Value&v){out+=selector(p_->segments[k],point)*v(0);});return out;}
const KMetrics&PublicK::metrics()const{return p_->metrics;}
void PublicK::report()const{auto&m=p_->metrics;std::cerr<<"OPT3C_K {\"K_initial_builds\":"<<m.initial_builds<<",\"fallback_rebuild_calls\":"<<m.fallback_rebuild_calls<<",\"K_fold_coordinates\":"<<m.fold_coordinates<<",\"K_state_read_coordinates\":"<<m.state_read_coordinates<<",\"K_initial_build_seconds\":"<<m.initial_build_seconds<<",\"K_fold_cpu_seconds\":"<<m.fold_cpu_seconds<<",\"K_fold_wall_seconds\":"<<m.fold_wall_seconds<<",\"K_file_read_bytes\":"<<m.file_read_bytes<<",\"K_file_write_bytes\":"<<m.file_write_bytes<<",\"K_RAM_peak_bytes\":"<<m.ram_peak_bytes<<",\"K_builder_peak_bytes\":"<<m.builder_peak_bytes<<",\"K_dirty_write_window_bytes\":"<<dirtyWindow<<",\"K_file_peak_bytes\":"<<m.file_peak_bytes<<",\"K_io_wall_seconds\":"<<m.io_wall_seconds<<",\"ram_compactions\":"<<m.ram_compactions<<",\"ram_released_bytes\":"<<m.ram_released_bytes<<",\"compaction_copy_bytes\":"<<m.compaction_copy_bytes<<",\"compaction_seconds_in_fold_wall\":"<<m.compaction_seconds<<",\"RAM_root_admissions\":"<<m.ram_roots<<",\"file_root_admissions\":"<<m.file_roots<<",\"rebuild_root_visits\":"<<m.rebuild_roots<<",\"RAM_limit\":"<<p_->budget.ram<<",\"file_limit\":"<<p_->budget.file<<",\"files_allowed\":"<<(p_->files_allowed?"true":"false")<<",\"file_page_cache_cost_is_zero\":false}\n";}
}
