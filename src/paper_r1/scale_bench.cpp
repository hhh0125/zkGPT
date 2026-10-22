#include "wire.hpp"
#include <chrono>
#include <iostream>
#include <filesystem>
using namespace zkgpt::r1;
int main(int argc,char **argv){try{
 init();require(argc==3,"usage: r2_scale_bench POWER_OF_TWO_QUERY_LENGTH NEW_OUTPUT_DIRECTORY");
 auto n=std::stoull(argv[1]);require(n>=2 && n<=kMaxVector/2,"benchmark query size");log2exact(n);
 std::filesystem::path dir=argv[2];require(!std::filesystem::exists(dir),"benchmark output exists");
 auto start=std::chrono::steady_clock::now();
 auto master=PCSpec::make("r2/scale/master",2*n,2*n,Layout::RowMajor);
 auto ts=PCSpec::make("r2/scale/t",n,n,Layout::ColumnMajor),es=PCSpec::make("r2/scale/E",n,n,Layout::RowMajor);
 LookupBatchSpec s;s.input={"R2-scale-mathematical-workload-not-model",Commitment{master,{}},
  {{Commitment{ts,{}},{{0,n,Fr(0)}}},{Commitment{es,{}},{{n,n,Fr(0)}}}},{{0,Fr(0)},{n,Fr(1048576)}}};
 s.ranges={{1,"r2/scale/range64",64}};s.exps={{0,1,"r2/scale/exp"}};
 auto resources=planLookupBatch(s);requireLookupBudget(s,std::size_t(2)<<30);
 std::filesystem::create_directories(dir);
 std::vector<Fr>w(2*n),t(n),e(n);const std::uint32_t boundary[]={0,1,6890,6891,8190,8191,655359,4294967295U};
 for(std::size_t i=0;i<n;++i){auto index=boundary[i%8];t[i]=Fr(index);e[i]=Fr(expValue(index));w[i]=t[i];w[n+i]=e[i];}
 s.input.master=commit(master,w);s.input.children[0].commitment=commit(ts,t);s.input.children[1].commitment=commit(es,e);
 Fs pt("lookup"),vt("lookup");auto p=proveLookupBatch(s,w,{t,e},pt);verifyLookupBatch(s,p,vt);
 require(pt.digest()==vt.digest(),"scale transcript divergence");auto sb=serializeLookup(s),pb=serializeLookup(s,p);
 auto decoded=parseLookupProof(s,pb);Fs wire("lookup");verifyLookupBatch(s,decoded,wire);
 writeFile((dir/"valid.statement").string(),sb);writeFile((dir/"valid.proof").string(),pb);
 auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 std::cout<<"{\"query_length\":"<<n<<",\"master_length\":"<<2*n<<",\"range_bits\":64,\"exp_index_bits\":32,\"table_length\":8192,\"estimated_prover_bytes\":"<<resources.estimated_prover_bytes<<",\"max_generators\":"<<resources.max_generators<<",\"max_sumcheck_rounds\":"<<resources.max_sumcheck_rounds<<",\"statement_bytes\":"<<sb.size()<<",\"proof_bytes\":"<<pb.size()<<",\"seconds\":"<<seconds<<",\"accepted\":true}\n";
 return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
