#include "single_hyrax.hpp"
#include "streaming_master.hpp"
#include "stage2_opening.hpp"
#include <algorithm>
#include <atomic>
#include <exception>
#include <mutex>
#include <thread>
#include <chrono>
#include <iostream>
namespace zkgpt::r7::oneblock::single {
namespace {
constexpr std::size_t rowChunk=256;
bool power(std::size_t n){return n && !(n&(n-1));}
void validatePlan(const Plan&p){
 need(power(p.domain) && p.domain<=maxDomain && power(p.column_limit) && p.column_limit<=hyrax_a3::maxDot,"single matrix domain/column limit");
 auto columns=std::min(p.column_limit,std::size_t(1)<<((bits(p.domain)+1)/2));
 need(p.columns==columns && p.rows==p.domain/columns && p.rows<=maxRows && p.source_identity.size()==64 && std::all_of(p.source_identity.begin(),p.source_identity.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}) && !p.segments.empty() && p.segments.size()<=8193,"single matrix noncanonical shape/identity/row budget");
 std::vector<unified::Segment>intervals;std::size_t actual=0;
 for(auto s:p.segments){need(s.actual && s.actual<=s.padded && s.actual<=p.domain && actual<=p.domain-s.actual,"single segment actual/overflow");actual+=s.actual;intervals.push_back({s.offset,s.padded});}
 need(p.segments[0].offset==0 && p.segments[0].padded<=std::size_t(1)<<24 && actual==p.logical_values,"single public X/effective length");
 unified::validate(p.domain,intervals);
}
void validatePublic(const Public&p){validatePlan(p.plan);need(p.rows.size()==p.plan.rows,"single row commitment count");}
void validateSecret(const Secret&s,const Public&p){
 need(descriptor(s.pc.plan)==descriptor(p.plan) && s.pc.rows==p.rows && s.blinds.size()==p.plan.rows && s.sources.size()==p.plan.segments.size(),"single private commitment identity/shape");
}
Bytes rowBytes(const Public&p,std::size_t start,std::size_t count){Writer w;w.number(count);for(std::size_t j=0;j<count;++j)w.point(p.rows.at(start+j));return w.b;}
// Returns whether any source intersects the row. Gaps remain zero. Source
// padded cells are read verbatim, so a malformed private tail is not sanitized.
bool readRow(const Secret&s,std::size_t row,std::vector<Fr>&out){
 const auto&p=s.pc.plan;out.assign(p.columns,Fr(0));auto begin=row*p.columns,end=begin+p.columns;bool touched=false;
 for(std::size_t k=0;k<p.segments.size();++k){auto seg=p.segments[k];auto lo=std::max(begin,seg.offset),hi=std::min(end,seg.offset+seg.padded);if(lo>=hi)continue;touched=true;
  for(auto j=lo;j<hi;++j){auto f=s.sources[k](j-seg.offset);need(f.isValid(),"single invalid private source scalar");out[j-begin]=f;}}
 return touched;
}
G1 rowCombination(const std::vector<G1>&rows,const std::vector<Fr>&weights){
 need(rows.size()==weights.size() && !rows.empty(),"single folded row dimensions");G1 out;out.clear();
 for(std::size_t at=0;at<rows.size();at+=16384){auto n=std::min<std::size_t>(16384,rows.size()-at);out+=master3::masterMSM(rows.data()+at,weights.data()+at,n);}return out;
}
struct Fold {hyrax_a3::DotWitness witness;std::vector<Fr>right;G1 commitment;};
Fold fold(const Public&p,const std::vector<Fr>&q,const Secret*s){
 need(q.size()==bits(p.plan.domain),"single query dimension");for(auto&v:q)need(v.isValid(),"single query scalar");
 const auto d=bits(p.plan.columns);auto left=r1::weights({q.begin()+d,q.end()});Fold out;out.right=r1::weights({q.begin(),q.begin()+d});out.commitment=rowCombination(p.rows,left);
 if(s){validateSecret(*s,p);out.witness.values.assign(p.plan.columns,Fr(0));std::vector<Fr>row;
  for(std::size_t i=0;i<p.plan.rows;++i){out.witness.blind+=left[i]*s->blinds[i];if(left[i].isZero() || !readRow(*s,i,row))continue;for(std::size_t j=0;j<row.size();++j)out.witness.values[j]+=left[i]*row[j];}}
 return out;
}
void proveFold(IO&io,const Public&p,const Query&q,Fold&f,const Hidden*h,Random*rng,const std::string&role){
 need(!role.empty() && role.size()<=160,"single opening role bound");crypto::validate(q.evaluation,io.session(),q.evaluation.role);
 Writer binding;binding.text(suite);binding.text(identity(p));binding.text(io.session());binding.text(role);binding.raw(crypto::encode(q.evaluation));for(auto&v:q.point)binding.scalar(v);
 bindPublic(io,role+"/binding",binding.b);
 if(io.prover()){need(h && rng && crypto::encode(h->handle)==crypto::encode(q.evaluation),"single hidden opening handle");f.witness.evaluation=*h;}
 stage2::ScopedIO scoped(io,role);hyrax_a3::dotProduct(scoped,f.commitment,q.evaluation,f.right,io.prover()?&f.witness:nullptr,rng);
}
struct Rule {std::size_t row,active=0;std::vector<std::pair<std::size_t,std::size_t>>private_ranges;};
Rule rule(const Plan&p,std::size_t row){Rule out{row,0,{}};auto begin=row*p.columns,end=begin+p.columns;
 for(std::size_t k=1;k<p.segments.size();++k){auto s=p.segments[k];auto lo=std::max(begin,s.offset),hi=std::min(end,s.offset+s.actual);if(lo<hi){out.private_ranges.emplace_back(lo-begin,hi-lo);out.active+=hi-lo;}}return out;}
G1 fixedPublic(const Plan&p,std::size_t row,const std::vector<Fr>&x,const std::vector<G1>&g){
 auto begin=row*p.columns;if(begin>=x.size()){G1 zero;zero.clear();return zero;}
 std::vector<Fr>v(p.columns,Fr(0));std::copy_n(x.begin()+begin,std::min(p.columns,x.size()-begin),v.begin());return master3::masterMSM(g,v);
}
}
unsigned bits(std::size_t n){need(power(n) && n<=maxDomain,"single metadata dimension");unsigned b=0;while(n>1){n>>=1;++b;}return b;}
Plan makePlan(const unified::Packing&packing,const std::vector<std::size_t>&actual,const std::string&source,std::size_t limit){
 need(actual.size()==packing.segments.size(),"single source/actual count");Plan p;p.source_identity=source;p.domain=packing.domain;p.column_limit=limit;
 need(power(limit) && limit<=hyrax_a3::maxDot,"single unsupported column budget");p.columns=std::min(limit,std::size_t(1)<<((bits(p.domain)+1)/2));p.rows=p.domain/p.columns;
 for(std::size_t k=0;k<actual.size();++k){auto s=packing.segments[k];p.segments.push_back({s.offset,actual[k],s.padded});need(actual[k]<=p.domain && p.logical_values<=p.domain-actual[k],"single logical length overflow");p.logical_values+=actual[k];}validatePlan(p);return p;
}
Bytes descriptor(const Plan&p){validatePlan(p);Writer w;w.text(suite);w.text(layout);w.text(modelScope);w.text(hyrax_a3::suite);w.text("same-A3-column-bases-and-Pedersen-H;no-trusted-W-bases");w.text(p.source_identity);
 for(auto n:{p.domain,p.rows,p.columns,p.logical_values,p.column_limit})w.number(n,8);w.number(p.segments.size());for(auto s:p.segments){w.number(s.offset,8);w.number(s.actual,8);w.number(s.padded,8);}return w.b;
}
Secret commit(const Plan&p,std::vector<Value>sources,Random&rng,unsigned workers){
 validatePlan(p);need(sources.size()==p.segments.size() && workers>0 && workers<=32,"single source/worker bound");for(auto&v:sources)need(bool(v),"single missing sealed source");
 const auto started=std::chrono::steady_clock::now();Secret s;s.pc.plan=p;s.sources=std::move(sources);s.pc.rows.resize(p.rows);s.blinds.reserve(p.rows);for(std::size_t i=0;i<p.rows;++i)s.blinds.push_back(rng.scalar());
 auto bases=hyrax_a3::generators(p.columns);auto H=parameters(1).hiding;workers=std::min<std::size_t>(workers,p.rows);
 std::atomic<std::size_t>next{0};std::atomic<bool>stop{false};std::exception_ptr failure;std::mutex failure_mutex;std::vector<std::uint64_t>calls(workers,0),terms(workers,0);
 auto work=[&](unsigned worker){try{std::vector<Fr>row;std::vector<G1>scratch(std::min<std::size_t>(1024,p.columns));for(;;){auto i=next.fetch_add(1);if(i>=p.rows || stop.load())break;G1 value;value.clear();if(readRow(s,i,row))for(std::size_t j=0;j<p.columns;j+=1024){auto n=std::min<std::size_t>(1024,p.columns-j);std::copy_n(bases.begin()+j,n,scratch.begin());G1 part;G1::mulVec(part,scratch.data(),row.data()+j,n);value+=part;++calls[worker];terms[worker]+=n;}s.pc.rows[i]=value+H*s.blinds[i];}}
 catch(...){stop.store(true);std::lock_guard<std::mutex>lock(failure_mutex);if(!failure)failure=std::current_exception();}};
 // Join even after a thread-creation failure; no background reads outlive s.
 std::vector<std::thread>pool;try{for(unsigned j=1;j<workers;++j)pool.emplace_back(work,j);work(0);}catch(...){stop.store(true);for(auto&t:pool)t.join();throw;}for(auto&t:pool)t.join();if(failure)std::rethrow_exception(failure);
 auto&m=metrics();for(unsigned j=0;j<workers;++j){m.msm_calls+=calls[j];m.msm_terms+=terms[j];}m.max_msm=std::max<std::uint64_t>(m.max_msm,std::min<std::size_t>(1024,p.columns));
 std::cerr<<"SINGLE_HYRAX_COMMIT {\"logical_roots\":1,\"rows\":"<<p.rows<<",\"columns\":"<<p.columns<<",\"domain\":"<<p.domain<<",\"logical_values\":"<<p.logical_values<<",\"padding\":"<<p.domain-p.logical_values<<",\"workers\":"<<workers<<",\"row_scalar_scratch_bound_bytes\":"<<workers*p.columns*sizeof(Fr)<<",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<"}\n";return s;
}
Bytes encode(const Public&p){validatePublic(p);Writer w;auto header=descriptor(p.plan);w.number(header.size());w.raw(header);auto out=std::move(w.b);out.reserve(out.size()+32*p.rows.size());for(std::size_t i=0;i<p.rows.size();i+=rowChunk){auto chunk=rowBytes(p,i,std::min(rowChunk,p.rows.size()-i));out.insert(out.end(),chunk.begin()+4,chunk.end());}return out;}
Public decode(const Bytes&b,const Plan&expected){auto header=descriptor(expected);Reader r{b};need(r.number()==header.size() && r.raw(header.size())==header,"single untrusted suite/layout/shape");need(b.size()-r.at==expected.rows*32,"single encoded row bytes/EOF");Public p;p.plan=expected;p.rows.reserve(expected.rows);for(std::size_t i=0;i<expected.rows;++i)p.rows.push_back(r.point());r.end();return p;}
std::string identity(const Public&p){validatePublic(p);Writer w;w.raw(descriptor(p.plan));w.number(rowChunk);for(std::size_t i=0;i<p.rows.size();i+=rowChunk){auto digest=r1::hash(rowBytes(p,i,std::min(rowChunk,p.rows.size()-i)));w.raw(Bytes(digest.begin(),digest.end()));}return hex(r1::hash(w.b));}
void writeLayout(std::ostream&out,const Public&p){validatePublic(p);out<<"{\"suite\":\""<<suite<<"\",\"layout\":\""<<layout<<"\",\"model_scope\":\""<<modelScope<<"\",\"logical_commitments\":1,\"identity\":\""<<identity(p)<<"\",\"source_layout_identity\":\""<<p.plan.source_identity<<"\",\"rows\":"<<p.plan.rows<<",\"columns\":"<<p.plan.columns<<",\"domain\":"<<p.plan.domain<<",\"logical_values\":"<<p.plan.logical_values<<",\"padding\":"<<p.plan.domain-p.plan.logical_values<<",\"source_segments\":[";for(std::size_t k=0;k<p.plan.segments.size();++k){if(k)out<<',';auto s=p.plan.segments[k];out<<"{\"storage_index\":"<<k<<",\"offset\":"<<s.offset<<",\"actual\":"<<s.actual<<",\"padded\":"<<s.padded<<'}';}out<<"]}\n";}
Public exchange(IO&io,const Plan&plan,const Secret*s){
 auto header=descriptor(plan);need(io.prover()?s!=nullptr:s==nullptr,"single PC P/V boundary");if(s){validatePublic(s->pc);need(descriptor(s->pc.plan)==header,"single PC secret plan");}
 bindPublic(io,"SingleHyrax/header",header);Public p;p.plan=plan;p.rows.reserve(plan.rows);
 for(std::size_t i=0;i<plan.rows;i+=rowChunk){auto n=std::min(rowChunk,plan.rows-i);auto b=io.message("SingleHyrax/rows/"+std::to_string(i),s?rowBytes(s->pc,i,n):Bytes{});Reader r{b};need(r.number()==n,"single row frame count");for(std::size_t j=0;j<n;++j)p.rows.push_back(r.point());r.end();}
 Writer seal;seal.text(suite);seal.text(identity(p));seal.text(io.session());bindPublic(io,"SingleHyrax/sealed",seal.b);return p;
}
FixedMetrics constrainFixed(IO&io,const Public&p,const std::vector<Fr>&x,const Secret*s,Random*rng){
 validatePublic(p);need(x.size()==p.plan.segments[0].actual && (io.prover()?(s && rng):(!s && !rng)),"single fixed-cell P/V/public X boundary");if(s)validateSecret(*s,p);for(auto&f:x)need(f.isValid(),"single invalid public X");
 Writer binding;binding.text(suite);binding.text(identity(p));binding.number(x.size(),8);bindPublic(io,"SingleHyrax/fixed-cells/statement",binding.b);
 for(std::size_t start=0;start<x.size();start+=65536){Writer chunk;auto n=std::min<std::size_t>(65536,x.size()-start);chunk.number(start,8);chunk.number(n);for(std::size_t j=0;j<n;++j)chunk.scalar(x[start+j]);bindPublic(io,"SingleHyrax/fixed-cells/public-X/"+std::to_string(start),chunk.b);}
 auto g=hyrax_a3::generators(p.plan.columns);auto H=parameters(1).hiding;FixedMetrics result;const auto batch_limit=std::max<std::size_t>(1,std::min<std::size_t>(64,65536/p.plan.columns));
 std::size_t cursor=0,batch=0;
 while(cursor<p.plan.rows){std::vector<Rule>rules;while(cursor<p.plan.rows && rules.size()<batch_limit){auto r=rule(p.plan,cursor++);if(r.active<p.plan.columns)rules.push_back(std::move(r));}if(rules.empty())continue;
  auto name="SingleHyrax/fixed-cells/"+std::to_string(batch++);Writer header;header.number(rules.size());for(auto&r:rules){header.number(r.row,8);header.number(r.active);}bindPublic(io,name+"/rows",header.b);
  std::vector<std::vector<Fr>>masks;std::vector<Fr>blinds;Writer first;
  if(io.prover())for(auto&r:rules){std::vector<Fr>d(r.active),row(p.plan.columns,Fr(0));std::size_t at=0;for(auto range:r.private_ranges)for(std::size_t j=0;j<range.second;++j){d[at]=rng->scalar();row[range.first+j]=d[at++];}auto rho=rng->scalar();G1 D;D.clear();if(r.active)D=master3::masterMSM(g,row);first.point(D+H*rho);masks.push_back(std::move(d));blinds.push_back(rho);}
  auto bytes=io.message(name+"/masks",first.b);Reader mr{bytes};std::vector<G1>D;for(std::size_t i=0;i<rules.size();++i)D.push_back(mr.point());mr.end();auto c=io.coin(name+"/challenge");need(!c.isZero(),"single zero fixed-cell challenge");Writer response;
  if(io.prover())for(std::size_t i=0;i<rules.size();++i){std::vector<Fr>row;readRow(*s,rules[i].row,row);std::size_t at=0;for(auto range:rules[i].private_ranges)for(std::size_t j=0;j<range.second;++j){response.scalar(masks[i][at++]+c*row[range.first+j]);}response.scalar(blinds[i]+c*s->blinds[rules[i].row]);}
  bytes=io.message(name+"/responses",response.b);result.response_bytes+=bytes.size();Reader rr{bytes};for(std::size_t i=0;i<rules.size();++i){std::vector<Fr>row(p.plan.columns,Fr(0));for(auto range:rules[i].private_ranges)for(std::size_t j=0;j<range.second;++j)row[range.first+j]=rr.scalar();auto rho=rr.scalar();if(!io.prover()){G1 A;A.clear();if(rules[i].active)A=master3::masterMSM(g,row);need(A+H*rho==D[i]+(p.rows[rules[i].row]-fixedPublic(p.plan,rules[i].row,x,g))*c,"single public-X/padding Sigma equation failed");}++result.rows;}rr.end();
 }
 Writer end;end.number(result.rows,8);end.number(result.response_bytes,8);bindPublic(io,"SingleHyrax/fixed-cells/complete",end.b);return result;
}
Fr evaluate(const Secret&s,const std::vector<Fr>&q){validatePublic(s.pc);auto f=fold(s.pc,q,&s);return r1::dot(f.witness.values,f.right);}
void open(IO&io,const Public&p,const Query&q,const Secret*s,const Hidden*h,Random*rng,const std::string&role){validatePublic(p);need(io.prover()?(s && h && rng):(!s && !h && !rng),"single opening P/V boundary");auto f=fold(p,q.point,s);proveFold(io,p,q,f,h,rng,role);}
Hidden evaluateAndOpen(IO&io,const Public&p,const std::vector<Fr>&q,const Secret*s,Random*rng,const std::string&role){validatePublic(p);need(io.prover()?(s && rng):(!s && !rng),"single evaluation P/V boundary");auto f=fold(p,q,s);auto h=exchangeHidden(io,role+"/evaluation",io.prover()?r1::dot(f.witness.values,f.right):Fr(0),rng);proveFold(io,p,{q,h.handle},f,io.prover()?&h:nullptr,rng,role);return h;}
}
