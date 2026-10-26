#include "sigma.hpp"
namespace zkgpt::r7::crypto {
namespace {
Hidden exchangeHandle(IO&io,const std::string&role,const Fr&value,Random*rng,const std::string&attack=""){
 Hidden h;Bytes b;if(io.prover()){h=hide(io.session(),role,value,*rng);if(attack=="swap-handle")h.handle.role+="/substituted";b=encode(h.handle);}
 b=io.message(role+"/commitment",b);h.handle=decodeHandle(b);validate(h.handle,io.session(),role);return h;
}
std::vector<Fr> selector(std::size_t index,std::size_t n){std::vector<Fr>r;for(unsigned i=0;i<r1::log2exact(n);++i)r.push_back(Fr((index>>i)&1));return r;}
std::vector<Fr> polynomial(const std::vector<std::vector<Fr>>&f){std::vector<Fr>out(f.size()+1,Fr(0));for(std::size_t j=0;j<f[0].size()/2;++j){std::vector<Fr>p{Fr(1)};for(auto&v:f){std::vector<Fr>q(p.size()+1,Fr(0));for(std::size_t k=0;k<p.size();++k){q[k]+=p[k]*v[2*j];q[k+1]+=p[k]*(v[2*j+1]-v[2*j]);}p=std::move(q);}for(std::size_t k=0;k<p.size();++k)out[k]+=p[k];}return out;}
}
void openingExample(IO&io,std::size_t n,const PCSecret*secret,Random*rng,bool wrong){
 PC pc;Bytes b;if(io.prover())b=encode(secret->public_pc);b=io.message("local-opening/PC",b);pc=decodePC(b);auto expected=PCSpec::make("local-opening/W",n,n);need(pc.spec.role==expected.role && pc.spec.actual==n && pc.spec.padded==n,"R7 opening expected PC shape");
 std::vector<Fr>point;for(unsigned i=0;i<r1::log2exact(n);++i)point.push_back(io.coin("local-opening/query/"+std::to_string(i)));
 auto h=exchangeHandle(io,"local-opening/hidden-evaluation",io.prover()?r1::mle(secret->values,point)+Fr(wrong?1:0):Fr(0),rng);
 std::vector<Query>queries{{point,h.handle}};std::vector<Hidden>w{h};opening(io,pc,queries,secret,io.prover()?&w:nullptr,rng);
}
void committedSumcheck(IO&io,const SumcheckInput&s,const PCSecret*secret,Random*rng,const std::string&attack){
 need((s.degree==2 || s.degree==3) && s.n && !(s.n&(s.n-1)) && s.n<=1024 && s.target<s.pc.spec.actual,"R7 committed Sumcheck dimensions");
 need(s.target==s.degree*s.n,"R7 exact factor/target layout required");
 Writer publicInput;publicInput.raw(encode(s.pc));publicInput.number(s.n);publicInput.number(s.degree);publicInput.number(s.target);publicInput.text(s.role);bindPublic(io,s.role+"/instance",publicInput.b);
 std::vector<std::vector<Fr>>f(s.degree,std::vector<Fr>(s.n,Fr(0)));if(io.prover()){need(secret && encode(secret->public_pc)==encode(s.pc),"R7 Sumcheck PC identity");for(std::size_t k=0;k<s.degree;++k)for(std::size_t j=0;j<s.n;++j)f[k][j]=secret->values.at(k*s.n+j);}
 auto initial=exchangeHandle(io,s.role+"/initial",io.prover()?secret->values.at(s.target):Fr(0),rng);std::vector<Hidden>all{initial};
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
 std::vector<Hidden>endpoints{term};for(std::size_t k=0;k<s.degree;++k)endpoints.push_back(exchangeHandle(io,s.role+"/endpoint/"+std::to_string(k),io.prover()?f[k][0]:Fr(0),rng,attack=="swap-handle"?attack:""));
 Fr product=io.prover()?endpoints[1].value*endpoints[2].value:Fr(0);if(io.prover() && attack=="bad-mul")product+=Fr(1);
 endpoints.push_back(exchangeHandle(io,s.role+"/product/0",product,rng));std::vector<std::array<std::size_t,3>>products{{1,2,endpoints.size()-1}};
 if(s.degree==3){auto intermediate=endpoints.size()-1;product=io.prover()?endpoints[intermediate].value*endpoints[3].value:Fr(0);endpoints.push_back(exchangeHandle(io,s.role+"/product/1",product,rng));products.push_back({intermediate,3,endpoints.size()-1});}
 Relations endpoint;endpoint.role=s.role+"/endpoint-relation";for(auto&h:endpoints)endpoint.handles.push_back(h.handle);endpoint.products=products;std::vector<Fr>line(endpoints.size(),Fr(0));line[0]=Fr(1);line.back()=Fr(-1);endpoint.linear.push_back(line);sigma(io,endpoint,io.prover()?&endpoints:nullptr,rng);
 std::vector<Query>queries;std::vector<Hidden>opened;
 queries.push_back({selector(s.target,s.pc.spec.padded),initial.handle});opened.push_back(initial);
 for(std::size_t k=0;k<s.degree;++k){auto q=point;auto high=selector(k,s.pc.spec.padded/s.n);q.insert(q.end(),high.begin(),high.end());queries.push_back({q,endpoints[k+1].handle});opened.push_back(endpoints[k+1]);}
 opening(io,s.pc,queries,secret,io.prover()?&opened:nullptr,rng);
}
}
