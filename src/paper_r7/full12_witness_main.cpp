#include "r6_adapter.hpp"
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
