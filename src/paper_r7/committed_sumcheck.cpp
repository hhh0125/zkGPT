#include "sigma.hpp"
namespace zkgpt::r7::crypto {
std::pair<Hidden,std::vector<Fr>> committedLinearSumcheck(IO&io,std::size_t n,const Hidden&initial,const std::function<Fr(std::size_t)>&source,const SparsePublicKernel&publicKernel,Random*rng,const std::string&role){
 need(n>=4096 && n<=(1u<<24) && !(n&(n-1)) && publicKernel.size()<=4096,"B1 sparse Sumcheck bounds");for(auto&t:publicKernel)need(t.first<n,"B1 public kernel index");
 auto previous=initial;auto kernel=publicKernel;std::vector<Fr>folded,point;std::size_t current=n;unsigned round=0;
 auto exchange=[&](const std::string&name,const Fr&value){Hidden h;Bytes b;if(io.prover()){h=hide(io.session(),name,value,*rng);b=encode(h.handle);}b=io.message(name+"/commitment",b);h.handle=decodeHandle(b);validate(h.handle,io.session(),name);return h;};
 while(current>1){auto name=role+"/round/"+std::to_string(round);std::vector<Fr>poly(3,Fr(0));if(io.prover()){
   auto read=[&](std::size_t i){return round==0?source(i):folded[i];};std::map<std::size_t,std::array<Fr,2>>pairs;for(auto&t:kernel)pairs.try_emplace(t.first/2,std::array<Fr,2>{Fr(0),Fr(0)}).first->second[t.first%2]=t.second;
   for(auto&t:pairs){auto x0=read(2*t.first),dx=read(2*t.first+1)-x0,k0=t.second[0],dk=t.second[1]-k0;poly[0]+=x0*k0;poly[1]+=x0*dk+dx*k0;poly[2]+=dx*dk;}
  }
  std::vector<Hidden>h{previous};for(unsigned j=0;j<3;++j)h.push_back(exchange(name+"/c/"+std::to_string(j),poly[j]));Relations relation;relation.role=name+"/recurrence";for(auto&x:h)relation.handles.push_back(x.handle);relation.linear={{Fr(-1),Fr(2),Fr(1),Fr(1)}};sigma(io,relation,io.prover()?&h:nullptr,rng);
  auto r=io.coin(name+"/r");point.push_back(r);std::vector<Hidden>coeff(h.begin()+1,h.end());previous=combine(io.session(),name+"/derived",coeff,{Fr(1),r,r*r});bindPublic(io,name+"/derived",encode(previous.handle));
  if(io.prover()){if(round==0){folded.resize(current/2);for(std::size_t j=0;j<current/2;++j){auto x=source(2*j);folded[j]=x+r*(source(2*j+1)-x);}}else{for(std::size_t j=0;j<current/2;++j)folded[j]=folded[2*j]+r*(folded[2*j+1]-folded[2*j]);folded.resize(current/2);}}
  SparsePublicKernel next;for(auto&t:kernel)next.try_emplace(t.first/2,Fr(0)).first->second+=t.second*((t.first&1)?r:Fr(1)-r);kernel=std::move(next);current/=2;++round;
 }
 auto end=exchange(role+"/master-terminal",io.prover()?folded[0]:Fr(0));auto publicValue=kernel.empty()?Fr(0):kernel.begin()->second;Relations terminal;terminal.role=role+"/terminal-link";terminal.handles={previous.handle,end.handle};terminal.linear={{Fr(1),-publicValue}};std::vector<Hidden>w{previous,end};sigma(io,terminal,io.prover()?&w:nullptr,rng);return {end,point};
}
namespace {
Hidden exchangeHandle(IO&io,const std::string&role,const Fr&value,Random*rng,const std::string&attack=""){
 Hidden h;Bytes b;if(io.prover()){h=hide(io.session(),role,value,*rng);if(attack=="swap-handle")h.handle.role+="/substituted";b=encode(h.handle);}
 b=io.message(role+"/commitment",b);h.handle=decodeHandle(b);validate(h.handle,io.session(),role);return h;
}
std::vector<Fr> selector(std::size_t index,std::size_t n){std::vector<Fr>r;for(unsigned i=0;i<r1::log2exact(n);++i)r.push_back(Fr((index>>i)&1));return r;}
std::vector<Fr> polynomial(const std::vector<std::vector<Fr>>&f){std::vector<Fr>out(f.size()+1,Fr(0));for(std::size_t j=0;j<f[0].size()/2;++j){std::vector<Fr>p{Fr(1)};for(auto&v:f){std::vector<Fr>q(p.size()+1,Fr(0));for(std::size_t k=0;k<p.size();++k){q[k]+=p[k]*v[2*j];q[k+1]+=p[k]*(v[2*j+1]-v[2*j]);}p=std::move(q);}for(std::size_t k=0;k<p.size();++k)out[k]+=p[k];}return out;}
}
void openingExample(IO&io,std::size_t n,const PCSecret*secret,Random*rng,bool wrong,std::size_t count){
 PC pc;Bytes b;if(io.prover())b=encode(secret->public_pc);b=io.message("local-opening/PC",b);pc=decodePC(b);auto expected=PCSpec::make("local-opening/W",n,n);need(pc.spec.role==expected.role && pc.spec.actual==n && pc.spec.padded==n,"R7 opening expected PC shape");
 std::vector<Query>queries;std::vector<Hidden>w;need(count>=1 && count<=4,"R7 opening example K bound");
 for(std::size_t k=0;k<count;++k){
  std::vector<Fr>point;for(unsigned i=0;i<r1::log2exact(n);++i)point.push_back(io.coin("local-opening/query/"+std::to_string(k)+"/"+std::to_string(i)));
  auto h=exchangeHandle(io,"local-opening/hidden-evaluation/"+std::to_string(k),io.prover()?r1::mle(secret->values,point)+Fr(wrong?1:0):Fr(0),rng);
  queries.push_back({point,h.handle});w.push_back(h);
 }
 opening(io,pc,queries,secret,io.prover()?&w:nullptr,rng);
}
void committedSumcheck(IO&io,const SumcheckInput&s,const PCSecret*secret,Random*rng,const std::string&attack){
 need((s.degree==2 || s.degree==3) && s.n && !(s.n&(s.n-1)) && s.n<=1024 && s.target<s.pc.spec.actual,"R7 committed Sumcheck dimensions");
 need(s.target==s.degree*s.n,"R7 exact factor/target layout required");
 Writer publicInput;publicInput.raw(encode(s.pc));publicInput.number(s.n);publicInput.number(s.degree);publicInput.number(s.target);publicInput.text(s.role);bindPublic(io,s.role+"/instance",publicInput.b);
 struct PackedCSIPA final:TerminalOpeningBackend {
  const SumcheckInput&s;const PCSecret*secret;
  PackedCSIPA(const SumcheckInput&input,const PCSecret*w):s(input),secret(w){}
  TerminalBackendKind kind()const override{return TerminalBackendKind::CSIPAResearchV1;}
  void authenticate(IO&live,const Hidden&initial,const std::vector<Hidden>&factors,const std::vector<Fr>&point,Random*random)override{
   std::vector<Query>queries{{selector(s.target,s.pc.spec.padded),initial.handle}};std::vector<Hidden>opened{initial};
   for(std::size_t k=0;k<s.degree;++k){auto q=point;auto high=selector(k,s.pc.spec.padded/s.n);q.insert(q.end(),high.begin(),high.end());queries.push_back({q,factors[k].handle});opened.push_back(factors[k]);}
   opening(live,s.pc,queries,secret,live.prover()?&opened:nullptr,random);
  }
 }backend(s,secret);
 SumcheckWitness witness;witness.factors.assign(s.degree,std::vector<Fr>(s.n,Fr(0)));
 if(io.prover()){need(secret && encode(secret->public_pc)==encode(s.pc),"R7 Sumcheck PC identity");for(std::size_t k=0;k<s.degree;++k)for(std::size_t j=0;j<s.n;++j)witness.factors[k][j]=secret->values.at(k*s.n+j);witness.claim=secret->values.at(s.target);}
 committedSumcheckRelation(io,{s.n,s.degree,s.role},io.prover()?&witness:nullptr,backend,rng,attack);
}
void committedSumcheckRelation(IO&io,const SumcheckRelation&s,const SumcheckWitness*witness,TerminalOpeningBackend&backend,Random*rng,const std::string&attack){
 auto kind=backend.kind();need(kind==TerminalBackendKind::CSIPAResearchV1 || kind==TerminalBackendKind::HyraxA3Stage2V1 || kind==TerminalBackendKind::MasterBridgeA3V1,"R7 unsupported terminal backend INCOMPLETE");
 need((s.degree==2 || s.degree==3) && s.n && !(s.n&(s.n-1)) && s.n<=(kind==TerminalBackendKind::MasterBridgeA3V1?4096u:1024u),"R7 committed reduction dimensions");
 Writer choice;choice.text(kind==TerminalBackendKind::CSIPAResearchV1?"CSIPA-RESEARCH-v1":kind==TerminalBackendKind::HyraxA3Stage2V1?"HYRAX-A3-STAGE2-v1":"MASTER-BRIDGE-A3-v1");choice.number(s.n);choice.number(s.degree);bindPublic(io,s.role+"/opening-backend",choice.b);
 std::vector<std::vector<Fr>>f(s.degree,std::vector<Fr>(s.n,Fr(0)));if(io.prover()){need(witness && witness->factors.size()==s.degree && rng,"R7 reduction witness shape");f=witness->factors;for(auto&v:f)need(v.size()==s.n,"R7 reduction factor shape");}
 auto initial=exchangeHandle(io,s.role+"/initial",io.prover()?witness->claim:Fr(0),rng);std::vector<Hidden>all{initial};
 std::vector<std::vector<Fr>>lines;std::vector<Fr>previous(1,Fr(-1)),point;std::size_t current=s.n;
 for(unsigned round=0;current>1;++round){std::vector<Fr>coeff(s.degree+1,Fr(0));if(io.prover())coeff=polynomial(f);if(io.prover() && attack=="bad-recursion" && round==0)coeff[0]+=Fr(1);
  auto offset=all.size();for(std::size_t j=0;j<coeff.size();++j)all.push_back(exchangeHandle(io,s.role+"/round/"+std::to_string(round)+"/c/"+std::to_string(j),coeff[j],rng));
  previous.resize(all.size(),Fr(0));previous[offset]+=Fr(2);for(std::size_t j=1;j<coeff.size();++j)previous[offset+j]+=Fr(1);lines.push_back(previous);
  auto r=io.coin(s.role+"/round/"+std::to_string(round)+"/r");point.push_back(r);previous.assign(all.size(),Fr(0));Fr power=1;for(std::size_t j=0;j<coeff.size();++j){previous[offset+j]=-power;power*=r;}
  if(io.prover()){for(auto&v:f){for(std::size_t j=0;j<current/2;++j)v[j]=v[2*j]+r*(v[2*j+1]-v[2*j]);v.resize(current/2);}}
  current/=2;
 }
 Fr terminal=1;if(io.prover())for(auto&v:f)terminal*=v[0];auto term=exchangeHandle(io,s.role+"/terminal",terminal,rng);all.push_back(term);previous.resize(all.size(),Fr(0));previous.back()=Fr(1);lines.push_back(previous);
 Relations recurrence;recurrence.role=s.role+"/sumcheck-relation";for(auto&h:all)recurrence.handles.push_back(h.handle);for(auto&line:lines)line.resize(all.size(),Fr(0));recurrence.linear=lines;sigma(io,recurrence,io.prover()?&all:nullptr,rng);
 std::vector<Hidden>endpoints{term};for(std::size_t k=0;k<s.degree;++k){auto value=io.prover()?f[k][0]:Fr(0);if(io.prover() && attack=="bad-endpoint" && k==0)value+=Fr(1);endpoints.push_back(exchangeHandle(io,s.role+"/endpoint/"+std::to_string(k),value,rng,attack=="swap-handle"?attack:""));}
 Fr product=io.prover()?endpoints[1].value*endpoints[2].value:Fr(0);if(io.prover() && attack=="bad-mul")product+=Fr(1);
 endpoints.push_back(exchangeHandle(io,s.role+"/product/0",product,rng));std::vector<std::array<std::size_t,3>>products{{1,2,endpoints.size()-1}};
 if(s.degree==3){auto intermediate=endpoints.size()-1;product=io.prover()?endpoints[intermediate].value*endpoints[3].value:Fr(0);endpoints.push_back(exchangeHandle(io,s.role+"/product/1",product,rng));products.push_back({intermediate,3,endpoints.size()-1});}
 Relations endpoint;endpoint.role=s.role+"/endpoint-relation";for(auto&h:endpoints)endpoint.handles.push_back(h.handle);endpoint.products=products;std::vector<Fr>line(endpoints.size(),Fr(0));line[0]=Fr(1);line.back()=Fr(-1);endpoint.linear.push_back(line);sigma(io,endpoint,io.prover()?&endpoints:nullptr,rng);
 std::vector<Hidden>factors(endpoints.begin()+1,endpoints.begin()+1+s.degree);backend.authenticate(io,initial,factors,point,rng);
}
}
