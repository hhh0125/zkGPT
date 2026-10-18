#include "exp_lookup.hpp"
#include "range_serialization.hpp"
#include "model_binding.hpp"
#include <cmath>
#include <iostream>
#include <functional>
void check(bool b,const std::string &s){if(!b)throw std::runtime_error(s);}
int main(){try{
 initPairing(mcl::BN254);setenv("ZKGPT_THREADS","2",1);
 for(size_t i=0;i<655360;i++)check(canonicalExpValue(i)==uint32_t(std::max(1.0,std::round(std::exp(-double(i)/512)*1048576))),"table generation mismatch");
 check(canonicalExpValue(0)==1048576 && canonicalExpValue(6890)==2 && canonicalExpValue(6891)==1 && canonicalExpValue(0xffffffffULL)==1,"table boundaries");
 RangePublicStatement s;s.shape={32,1,1,1,1,1};s.val0_log_size=11;s.val0_generator_domain={"zkGPT/main",64};
 const size_t count=528; s.regions={{WitnessKind::SOFTMAX,"softmax_0_t",0,count,32,false,0,0},{WitnessKind::SOFTMAX,"softmax_0_E",count,count,32,false,1,0}};
 const auto &g=getGeneratorSet(s.val0_generator_domain);std::vector<Fr>w(2048,Fr(0));uint64_t indices[]={0,1,512,6890,6891,655359,655360,0xffffffffULL};for(size_t i=0;i<count;i++){w[i]=Fr(indices[i%8]);w[count+i]=Fr(canonicalExpValue(indices[i%8]));}
 auto commit=[&]{s.val0_commitment.resize(32);for(size_t row=0;row<32;row++)G1::mulVec(s.val0_commitment[row],const_cast<G1*>(g.generators.data()),w.data()+row*64,64);};commit();
 std::string e;auto p=proveExpLookup(s,w,Fr(123),2);check(verifyExpLookup(s,p,Fr(123),&e),"positive: "+e);std::cout<<"PASS positive all boundary/tail/repeated queries\n";
 auto b=serializeRangeProof(p.payload);RangeProof decoded;check(deserializeRangeProof(b,decoded,&e),"decode");check(serializeRangeProof(decoded)==b,"canonical roundtrip");p.payload=decoded;check(verifyExpLookup(s,p,Fr(123),&e),"decoded verification");std::cout<<"PASS serialized proof roundtrip bytes="<<b.size()<<"\n";
 auto reject=[&](ExpLookupProof a,RangePublicStatement z,const Fr &ctx,const char *name){check(!verifyExpLookup(z,a,ctx,&e),std::string("accepted ")+name);std::cout<<"REJECT "<<name<<": "<<e<<"\n";};
 auto a=p;a.payload.membership_proofs[0][6].query_index=1;reject(a,s,Fr(123),"noncanonical normalization shell");a=p;a.exp_table_id[0]^=1;reject(a,s,Fr(123),"table identity");a=p;a.query_count--;reject(a,s,Fr(123),"delete query");a=p;a.payload.membership_proofs[0][0].reciprocal_sum+=Fr(1);reject(a,s,Fr(123),"lookup proof");a=p;a.payload.chunk_commitments[1][0][0]+=g.generators[0];reject(a,s,Fr(123),"lookup commitment");a=p;a.payload.transcript_binding+=Fr(1);reject(a,s,Fr(123),"transcript");reject(p,s,Fr(124),"parent transcript");auto z=s;z.regions[0].val0_offset++;reject(p,z,Fr(123),"registry index");z=s;z.regions.pop_back();reject(p,z,Fr(123),"delete last layer");z=s;z.regions[0].count--;reject(p,z,Fr(123),"omit query region");z=s;z.regions[0].proof_start++;reject(p,z,Fr(123),"region order metadata");
 auto honest=w;auto attack=[&](const char *name,std::function<void()> mutate){w=honest;mutate();commit();bool failed=false;try{auto bad=proveExpLookup(s,w,Fr(123),2);failed=!verifyExpLookup(s,bad,Fr(123),&e);}catch(const std::exception &x){failed=true;e=x.what();}check(failed,std::string("malicious accepted ")+name);std::cout<<"REJECT malicious witness "<<name<<": "<<e<<"\n";
 // Produce a full format-valid dishonest proof after bypassing local equality.
 auto forged=proveExpLookup(s,w,Fr(123),2,true);auto bytes=serializeRangeProof(forged.payload);RangeProof fp;check(deserializeRangeProof(bytes,fp,&e),"forged decode");forged.payload=fp;check(!verifyExpLookup(s,forged,Fr(123),&e),"forged proof accepted");check(e.find("degree-1 sumcheck")!=std::string::npos,"missing precise forged-proof rejection diagnostic: "+e);std::cout<<"REJECT bypassed prover checks "<<name<<": "<<e<<"\n";};
 attack("E+1",[&]{w[count]+=Fr(1);});attack("scale x2",[&]{for(size_t i=0;i<count;i++)w[count+i]*=Fr(2);});attack("t change keep E",[&]{w[0]=Fr(1);});attack("legal-range E",[&]{w[count+2]=Fr(17);});attack("swap E",[&]{std::swap(w[count],w[count+2]);});
 BoundArtifact legacy;check(!verifyBoundArtifact(ModelAnchor{},legacy,{},&e),"legacy v3 strict accepted");std::cout<<"REJECT legacy v3: "<<e<<"\nALL EXP LOOKUP TESTS PASS\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
