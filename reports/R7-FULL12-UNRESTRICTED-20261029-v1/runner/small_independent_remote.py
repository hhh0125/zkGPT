import subprocess,os,json
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1');env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2')
s=Path('src/paper_r7/full12_runtime_tests.cpp').read_text().replace('#include "stage2_opening.hpp"','#include "stage2_opening.hpp"\n#include "fs_io.hpp"')
s=s.replace('int main(){try{','int main(int argc,char**argv){try{need(argc==2,"component probe requires proof path");')
s=s.replace('SessionRandom rng;Tape tx(true);','''SessionRandom rng;namespace fs=zkgpt::r7::fs;fs::Binding binding{"Full12","synthetic-partition-component-not-native-Full12",std::string(64,'a'),{3,1,4,1,5}};
#ifdef R7_FULL12_PROBE_VERIFIER
 constexpr bool isP=false;fs::FsVerifierIO tx(argv[1],binding);
#else
 constexpr bool isP=true;fs::FsProverIO tx(argv[1],binding);
#endif
''')
s=s.replace('secrets.push_back(model::rerandomize(trust.back(),tx.session(),std::move(values),rng));','if(isP)secrets.push_back(model::rerandomize(trust.back(),tx.session(),std::move(values),rng));')
s=s.replace('Commitments p(tx,l,base,&prepared,&rng,nullptr,nullptr,&trust,&secrets);','Commitments p(tx,l,isP?base:Value{},isP?&prepared:nullptr,isP?&rng:nullptr,nullptr,nullptr,&trust,isP?&secrets:nullptr);')
old='std::vector<Fr>values(v.padded);for(std::size_t j=0;j<v.padded;++j)values[j]=v.value(j);auto h=exchangeHidden(tx,std::string(id)+"/value",zkgpt::r1::mle(values,point),&rng);v.open(tx,{point,h.handle},&h,&rng,std::string(id)+"/open");'
new='Fr value=0;if(isP){std::vector<Fr>values(v.padded);for(std::size_t j=0;j<v.padded;++j)values[j]=v.value(j);value=zkgpt::r1::mle(values,point);}auto h=exchangeHidden(tx,std::string(id)+"/value",value,isP?&rng:nullptr);v.open(tx,{point,h.handle},isP?&h:nullptr,isP?&rng:nullptr,std::string(id)+"/open");'
assert old in s;s=s.replace(old,new)
start=s.index(' Tape rx(false);');end=s.index('}catch(',start)
s=s[:start]+''' need(p.authenticated(),"component authentication barrier");tx.finish();std::cerr<<tx.metadataJson()<<'\\n';std::cout<<(isP?"PARTITION_COMPONENT_PROOF_CREATED":"PARTITION_COMPONENT_INDEPENDENT_VERIFIED")<<'\\n';return 0;
'''+s[end:]
Path('src/paper_r7/full12_runtime_probe.cpp').write_text(s)
p=Path('src/paper_r7/CMakeLists.txt');p.write_text(p.read_text()+'''
add_executable(r7_full12_component_prover full12_runtime_probe.cpp)
target_link_libraries(r7_full12_component_prover PRIVATE paper_r7)
add_executable(r7_full12_component_verifier full12_runtime_probe.cpp)
target_compile_definitions(r7_full12_component_verifier PRIVATE R7_FULL12_PROBE_VERIFIER=1)
target_link_libraries(r7_full12_component_verifier PRIVATE paper_r7)
''')
def run(name,cmd,expected=0):
 with (stage/(name+'.log')).open('xb') as out:r=subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT,env=env)
 (stage/(name+'.json')).write_text(json.dumps(dict(command=cmd,exit_code=r.returncode,expected_exit_code=expected),indent=2));print(name,r.returncode,(stage/(name+'.log')).read_text()[-1600:],flush=True);assert r.returncode==expected
run('small-configure',['cmake','-S','.','-B','build/release'])
run('small-build',['cmake','--build','build/release','--target','r7_full12_component_prover','r7_full12_component_verifier','-j4'])
proof=stage/'small-partition.fsproof'
run('small-independent-P',['build/release/src/paper_r7/r7_full12_component_prover',str(proof)])
run('small-independent-V',['build/release/src/paper_r7/r7_full12_component_verifier',str(proof)])
raw=proof.read_bytes();bad=stage/'small-partition-tampered.fsproof';raw=bytearray(raw);raw[-1]^=1;bad.write_bytes(raw)
run('small-tampered-V',['build/release/src/paper_r7/r7_full12_component_verifier',str(bad)],1)
