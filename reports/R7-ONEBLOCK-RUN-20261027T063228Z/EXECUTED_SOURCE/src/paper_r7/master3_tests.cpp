#include "streaming_master.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace master3;
int main(){try{
 r1::init();unsigned count=0;auto check=[&](bool b){need(b,"M3 unit assertion");++count;};auto rejects=[&](auto f){try{f();}catch(const std::exception&){++count;return;}throw std::runtime_error("M3 expected rejection");};
 auto c=trustedContext();auto l=trustedLayout(c);check(l.actual==16642424 && l.padded==(1u<<24) && l.object_count==331);check(encodeLayout(l)==encodeLayout(trustedLayout(c)));check(l.projection.size()==2050);check(l.views.size()==332);bool paddedShape=false,alias=false;for(auto&r:l.regions){if(r.id=="R6/parameter/fc_0.weight")paddedShape=r.rows==4096 && r.columns==1024 && r.logical_rows==2304 && r.logical_columns==768;if(r.alias=="R6/witness/r5b_input")alias=true;}check(paddedShape && alias);
 auto bad=l;bad.regions[1].offset=0;rejects([&]{validate(bad);});bad=l;bad.regions[0].count=0;rejects([&]{validate(bad);});bad=l;bad.regions[0].offset=~std::uint64_t(0);rejects([&]{validate(bad);});bad=l;bad.regions[1].id=bad.regions[0].id;rejects([&]{validate(bad);});bad=l;bad.regions.pop_back();rejects([&]{validate(bad);});
 OSRandom rng;auto spec=makeSpec(l,4096,true);auto value=[](std::size_t i){return Fr(int(i%17)-8);};auto s=commitMaster(spec,value);check(s.public_pc.rows.size()==4 && s.blinds.size()==4);
 auto bases=hyrax_a3::generators(1024);for(std::size_t row=0;row<4;++row){G1 ref=parameters(1).hiding*s.blinds[row];for(std::size_t j=0;j<1024;++j){auto k=row+4*j;if(k<spec.actual)ref+=bases[j]*value(k);}check(ref==s.public_pc.rows[row]);}
 std::vector<Fr>reference(4096,Fr(0)),point(12,Fr(3));for(std::size_t j=0;j<spec.actual;++j)reference[j]=value(j);check(evaluate(s,point)==r1::mle(reference,point));
 auto other=commitMaster(spec,value);check(encodeMaster(s.public_pc)!=encodeMaster(other.public_pc));check(encodeMaster(decodeMaster(encodeMaster(s.public_pc),spec))==encodeMaster(s.public_pc));auto bytes=encodeMaster(s.public_pc);bytes.push_back(0);rejects([&]{decodeMaster(bytes,spec);});auto wrong=spec;wrong.layout=std::string(64,'0');rejects([&]{decodeMaster(encodeMaster(s.public_pc),wrong);});
 auto m=testMap("repeat");std::vector<Fr>q(4,Fr(2));Fr bias;auto kernel=gatherKernel(m,q,4096,bias);auto w=r1::weights(q);check(kernel[3]==w[0]+w[1]+w[2]);auto inv=m;inv.terms[0].source=4096;rejects([&]{validate(inv,4096);});inv=m;inv.terms[0].child=16;rejects([&]{validate(inv,4096);});
 check(crypto::maxN==4096 && spec.columns==1024);std::cout<<"M3_UNIT_PASSED "<<count<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
