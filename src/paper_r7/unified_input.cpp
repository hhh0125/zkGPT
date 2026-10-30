#include "unified_input.hpp"
#include <algorithm>
#include <map>
#include <chrono>
#include <iostream>
#include "opt2_metrics.hpp"
namespace zkgpt::r7::oneblock::unified {
namespace {
constexpr std::size_t maxDomain=std::size_t(1)<<32;
bool power(std::size_t n){return n && !(n&(n-1));}
// The virtual domain is never materialized. Do not apply r1::log2exact's
// separate 2^28 allocation bound to its metadata-only round count.
unsigned domainBits(std::size_t n){need(power(n) && n<=maxDomain,"unified virtual domain dimension bound");unsigned bits=0;while(n>1){n>>=1;++bits;}return bits;}
class SplitWeights {
 std::vector<Fr>low_,high_;unsigned split_;
 public:explicit SplitWeights(const std::vector<Fr>&q):split_(q.size()/2){low_=r1::weights({q.begin(),q.begin()+split_});high_=r1::weights({q.begin()+split_,q.end()});}
 Fr at(std::size_t j)const{return low_[j&(low_.size()-1)]*high_[j>>split_];}
};
Fr fixedBits(const Segment&s,const std::vector<Fr>&point,unsigned begin){Fr weight=1;for(unsigned j=begin;j<point.size();++j)weight*=((s.offset>>j)&1)?point[j]:Fr(1)-point[j];return weight;}
}
void validate(std::size_t n,const std::vector<Segment>&segments){
 need(power(n) && n<=maxDomain && !segments.empty() && segments.size()<=8193,"unified input domain/segment bound");
 auto sorted=segments;std::sort(sorted.begin(),sorted.end(),[](auto a,auto b){return a.offset<b.offset;});std::size_t end=0;
 for(auto s:sorted){need(power(s.padded) && s.padded<=n && s.offset<=n-s.padded && s.offset%s.padded==0 && s.offset>=end,"unified unaligned/overlapping/out-of-domain segment");end=s.offset+s.padded;}
}
Packing pack(const std::vector<Block>&blocks,std::size_t public_padded){
 need(power(public_padded) && public_padded<=maxDomain/4 && blocks.size()<=8192,"unified public input packing bound");
 Packing out;std::array<std::size_t,4>cursor{public_padded,0,0,0};out.segments.push_back({0,public_padded});
 for(auto b:blocks){need(b.category>=1 && b.category<=3 && power(b.padded) && b.padded<=maxDomain/4,"unified private category/block bound");auto&at=cursor[b.category];need(at<=maxDomain/4-(b.padded-1),"unified packing alignment overflow");auto begin=(at+b.padded-1)&~(b.padded-1);need(begin<=maxDomain/4-b.padded,"unified partition bound");out.segments.push_back({begin,b.padded});at=begin+b.padded;}
 auto longest=*std::max_element(cursor.begin(),cursor.end());out.partition_size=1;while(out.partition_size<longest)out.partition_size*=2;out.domain=4*out.partition_size;
 for(std::size_t k=0;k<blocks.size();++k)out.segments[k+1].offset+=blocks[k].category*out.partition_size;
 validate(out.domain,out.segments);return out;
}
Fr selector(const Segment&s,const std::vector<Fr>&point){need(power(s.padded) && point.size()>=r1::log2exact(s.padded) && point.size()<=32,"unified selector dimensions");return fixedBits(s,point,r1::log2exact(s.padded));}
Round quadraticRound(std::size_t n,const std::vector<Segment>&segments,const std::vector<Fr>&prefix,const Factors&provider){
 validate(n,segments);need(prefix.size()<domainBits(n) && provider,"unified round prefix/provider");const auto t=unsigned(prefix.size());Round out;
 struct Pair{Fr a=0,b=0;};std::map<std::size_t,Pair>collapsed;
 auto product=[&](const Fr&a0,const Fr&a1,const Fr&b0,const Fr&b1){auto da=a1-a0,db=b1-b0;out.coefficients[0]+=a0*b0;out.coefficients[1]+=a0*db+da*b0;out.coefficients[2]+=da*db;if((n>>t)==2){out.input_pair[0]+=a0;out.input_pair[1]+=a1;out.kernel_pair[0]+=b0;out.kernel_pair[1]+=b1;}};
 for(std::size_t k=0;k<segments.size();++k){auto s=segments[k];auto d=r1::log2exact(s.padded);auto low=std::min<unsigned>(t,d);SplitWeights weights({prefix.begin(),prefix.begin()+low});const auto block=std::size_t(1)<<low;unsigned calls=0;
  provider(k,[&](const Value&a,const Value&b){++calls;need(calls==1 && a && b,"unified factor oracle invocation");
   auto folded=[&](std::size_t j){Pair v;for(std::size_t i=0;i<block;++i){auto index=j*block+i;if(!low){v.a=a(index);v.b=b(index);}else{auto w=weights.at(i);v.a+=w*a(index);v.b+=w*b(index);}}return v;};
   if(t<d){for(std::size_t j=0;j<(s.padded>>t)/2;++j){auto left=folded(2*j),right=folded(2*j+1);product(left.a,right.a,left.b,right.b);}}
   else{auto v=folded(0);auto w=fixedBits(s,prefix,d);auto&target=collapsed[s.offset>>t];target.a+=w*v.a;target.b+=w*v.b;}
  });need(calls==1,"unified missing factor oracle");
 }
 // Large aligned blocks occupy complete suffix pairs; scalar blocks can
 // collide, so combine both factors first (including X with a zero K block).
 for(auto it=collapsed.begin();it!=collapsed.end();){auto pair=it->first>>1;Pair left,right;do{if(it->first&1)right=it->second;else left=it->second;++it;}while(it!=collapsed.end() && (it->first>>1)==pair);product(left.a,right.a,left.b,right.b);}
 return out;
}
Reduction reduce(IO&io,std::size_t n,const std::vector<Segment>&segments,const Hidden&initial,
 const Factors&factors,const std::function<Fr(const std::vector<Fr>&)>&kernel_at,Random*rng,const std::string&role){
 auto started=std::chrono::steady_clock::now();auto cpu=crypto::processCPU();
 validate(n,segments);need(kernel_at && (io.prover()?bool(factors)&&rng:!factors&&!rng),"unified private prover/verifier boundary");
 crypto::validate(initial.handle,io.session(),initial.handle.role);
 Writer statement;statement.text(suite);statement.number(n,8);statement.number(2);statement.raw(encode(initial.handle));statement.number(segments.size());for(auto s:segments){statement.number(s.offset,8);statement.number(s.padded,8);}bindPublic(io,role+"/statement",statement.b);
 auto previous=initial;std::vector<Fr>point;Round last;
 for(unsigned t=0;t<domainBits(n);++t){auto name=role+"/round/"+std::to_string(t);if(io.prover())last=quadraticRound(n,segments,point,factors);
  std::vector<Hidden>h{previous};for(unsigned j=0;j<3;++j)h.push_back(exchangeHidden(io,name+"/c/"+std::to_string(j),last.coefficients[j],rng));
  Relations recurrence;recurrence.role=name+"/recurrence";for(auto&v:h)recurrence.handles.push_back(v.handle);recurrence.linear={{Fr(-1),Fr(2),Fr(1),Fr(1)}};sigma(io,recurrence,io.prover()?&h:nullptr,rng);
  auto r=io.coin(name+"/r");point.push_back(r);previous=combine(io.session(),name+"/derived",{h[1],h[2],h[3]},{Fr(1),r,r*r});bindPublic(io,name+"/derived",encode(previous.handle));
 }
 Fr input=0,actual_kernel=0;if(io.prover()){
  if(n==1){factors(0,[&](const Value&a,const Value&b){input=a(0);actual_kernel=b(0);});}
  else{auto r=point.back();input=last.input_pair[0]+r*(last.input_pair[1]-last.input_pair[0]);actual_kernel=last.kernel_pair[0]+r*(last.kernel_pair[1]-last.kernel_pair[0]);}
 }
 auto expected=kernel_at(point);if(io.prover())need(expected==actual_kernel,"unified independently evaluated public kernel mismatch");
 auto input_end=exchangeHidden(io,role+"/input-evaluation",input,rng);auto public_kernel=publicHidden(io,role+"/public-kernel",expected);bindPublic(io,role+"/public-kernel",encode(public_kernel.handle));
 std::vector<Hidden>w{input_end,public_kernel,previous};Relations terminal;terminal.role=role+"/terminal";for(auto&h:w)terminal.handles.push_back(h.handle);terminal.products={{0,1,2}};sigma(io,terminal,io.prover()?&w:nullptr,rng);
 std::cerr<<"UNIFIED_SUMCHECK {\"domain\":"<<n<<",\"rounds\":"<<point.size()<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<",\"cpu_seconds\":"<<crypto::processCPU()-cpu<<",\"includes_public_kernel_endpoint\":true}\n";
 return {initial,{input_end,public_kernel},point};
}
}
