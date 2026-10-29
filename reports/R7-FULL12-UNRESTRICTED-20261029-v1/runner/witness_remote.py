import subprocess,json,os,resource,time,signal
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1')
code=r'''#include "r6_adapter.hpp"
#include "execution_target.hpp"
#include "master_layout.hpp"
#include "paper_r1/wire.hpp"
#include <sys/prctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <cstring>
using namespace zkgpt::r7;
void writeAll(int fd,const void*data,std::size_t n){auto p=static_cast<const char*>(data);while(n){auto amount=std::min<std::size_t>(n,16u<<20);auto k=::write(fd,p,amount);crypto::need(k>0,"private checkpoint write failed");p+=k;n-=k;}}
void publicReference(const std::vector<Fr>&v,const std::string&file){auto b=zkgpt::r1::readFile(file);crypto::need(b.size()==v.size()*8,"Full12 public reference shape");for(std::size_t i=0;i<v.size();++i){std::uint64_t bits=0;for(unsigned j=0;j<8;++j)bits|=std::uint64_t(b[8*i+j])<<(8*j);std::int64_t value;std::memcpy(&value,&bits,8);crypto::need(v[i]==Fr(value),"Full12 public reference mismatch (values suppressed)");}}
int main(int argc,char**argv){try{
 crypto::need(argc==2 && prctl(PR_SET_DUMPABLE,0)==0,"usage: full12_witness PRIVATE_CHECKPOINT_DIRECTORY");zkgpt::r1::init();
 auto t=executionTarget(12);auto c=master3::trustedContext(12);R7WitnessStore w;
 std::cerr<<"FULL12_WITNESS_BEGIN blocks=12 master_expected=199348640 no_legacy_commitment=true\n";
 {master3::QuietStdout quiet;w.generateR6(c,t.manifest_path);}
 crypto::need(w.baseMaster().size()==199348640 && w.legacyCommitmentAbsent(),"Full12 master/no-legacy requirement");
 const std::string ref="reports/R6-FULL12-P0AB-20261021/D-EXECUTION-20261022/";
 publicReference(w.publicX(),ref+"FULL12_PUBLIC_X_SIGNED64_LE.bin");publicReference(w.publicY(c),ref+"FULL12_FINAL_BLOCK_Y_SIGNED64_LE.bin");
 auto root=std::filesystem::canonical(std::filesystem::current_path());auto dest=std::filesystem::weakly_canonical(std::filesystem::absolute(argv[1]));auto rel=dest.lexically_relative(root);
 crypto::need(!rel.empty() && *rel.begin()!=".." && rel.generic_string().rfind("build/private-full12/",0)==0 && !std::filesystem::exists(dest),"new private fixed-workspace directory required");
 std::filesystem::create_directories(dest);crypto::need(chmod(dest.c_str(),0700)==0,"private checkpoint permissions");
 auto path=dest/"witness.local.part";int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);crypto::need(fd>=0,"private checkpoint create");
 auto&pr=w.executionForR7();std::uint64_t count=pr.val.size();crypto::need(count==277,"all native layer witnesses retained");writeAll(fd,&count,8);std::uint64_t total=0;
 for(auto&v:pr.val){std::uint64_t n=v.size();writeAll(fd,&n,8);writeAll(fd,v.data(),v.size()*sizeof(Fr));total+=n;}
 crypto::need(fsync(fd)==0 && close(fd)==0,"private checkpoint flush");std::filesystem::rename(path,dest/"witness.local");
 std::cerr<<"FULL12_WITNESS_VALIDATED_AND_RETAINED blocks=12 layers=277 master=199348640 X_reference=match Y_reference=match legacy_commitment=absent fields="<<total<<" checkpoint_bytes="<<std::filesystem::file_size(dest/"witness.local")<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FULL12_WITNESS_INCOMPLETE "<<e.what()<<'\n';return 2;}}
'''
Path('src/paper_r7/full12_witness_main.cpp').write_text(code)
p=Path('src/paper_r7/CMakeLists.txt');s=p.read_text();assert 'add_executable(r7_full12_witness ' not in s;p.write_text(s+'\nadd_executable(r7_full12_witness full12_witness_main.cpp)\ntarget_link_libraries(r7_full12_witness PRIVATE paper_r7)\n')
env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2')
for name,cmd in [('configure-witness',['cmake','-S','.','-B','build/release']),('build-witness',['cmake','--build','build/release','--target','r7_full12_witness','-j4'])]:
 with (stage/(name+'.log')).open('xb') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,env=env)
 print(name,r.returncode,flush=True);assert r.returncode==0,(stage/(name+'.log')).read_text()[-3000:]
before=resource.getrlimit(resource.RLIMIT_AS)
def own_limits():
 soft,hard=resource.getrlimit(resource.RLIMIT_AS)
 if hard==resource.RLIM_INFINITY:resource.setrlimit(resource.RLIMIT_AS,(resource.RLIM_INFINITY,hard))
 else:resource.setrlimit(resource.RLIMIT_AS,(hard,hard))
 resource.setrlimit(resource.RLIMIT_CORE,(0,0));os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
cmd=['build/release/src/paper_r7/r7_full12_witness','build/private-full12/unrestricted-20261029-v1']
start=time.monotonic();cpu_before=resource.getrusage(resource.RUSAGE_CHILDREN);peak_rss=peak_vm=0;pressure=0
with (stage/'witness.log').open('xb') as out,(stage/'witness-resources.jsonl').open('x') as samples:
 p=subprocess.Popen(cmd,stdout=out,stderr=subprocess.STDOUT,env=env,preexec_fn=own_limits,start_new_session=True)
 while p.poll() is None:
  status={}
  try:
   for line in Path('/proc/'+str(p.pid)+'/status').read_text().splitlines():
    k,_,v=line.partition(':')
    if k in ['VmSize','VmPeak','VmRSS','VmHWM']:status[k]=int(v.split()[0])
   if not (stage/'WITNESS_PROCESS_LIMITS.txt').exists():(stage/'WITNESS_PROCESS_LIMITS.txt').write_text(Path('/proc/'+str(p.pid)+'/limits').read_text())
  except FileNotFoundError:pass
  mem={line.split(':')[0]:int(line.split()[1]) for line in Path('/proc/meminfo').read_text().splitlines() if line.startswith(('MemAvailable:','SwapFree:'))}
  peak_rss=max(peak_rss,status.get('VmHWM',0),status.get('VmRSS',0));peak_vm=max(peak_vm,status.get('VmPeak',0),status.get('VmSize',0))
  samples.write(json.dumps(dict(elapsed=time.monotonic()-start,pid=p.pid,status=status,system=mem))+'\n');samples.flush()
  pressure=pressure+1 if mem['MemAvailable']<32*1024*1024 else 0
  if pressure>=3:os.killpg(p.pid,signal.SIGTERM);print('OWN_TASK_STOPPED_SYSTEM_MEMORY_PRESSURE',flush=True);break
  time.sleep(5)
 code=p.wait()
cpu_after=resource.getrusage(resource.RUSAGE_CHILDREN)
result=dict(command=cmd,exit_code=code,wall_seconds=time.monotonic()-start,user_cpu_seconds=cpu_after.ru_utime-cpu_before.ru_utime,system_cpu_seconds=cpu_after.ru_stime-cpu_before.ru_stime,peak_rss_kib=peak_rss,peak_vmsize_kib=peak_vm,parent_rlimit_as=list(before),arbitrary_time_limit=None,pressure_stop=pressure>=3,private_checkpoint='build/private-full12/unrestricted-20261029-v1/witness.local',checkpoint_archived=False)
(stage/'WITNESS_RESULT.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2),flush=True);print((stage/'witness.log').read_text()[-2500:],flush=True)
assert code==0
