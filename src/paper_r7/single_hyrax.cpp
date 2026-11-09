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
#include <set>
#include <tuple>
namespace zkgpt::r7::oneblock::single {
namespace {
constexpr std::size_t rowChunk=256;
bool power(std::size_t n){return n && !(n&(n-1));}
std::vector<PhysicalRun>deriveRuns(const Plan&p){
 if(p.category_mode){need(p.public_cells.empty(),"category X cannot have public holes");std::vector<PhysicalRun>out;std::size_t at=0;for(std::size_t k=0;k<p.segments.size();++k){out.push_back({k,0,p.segments[k].actual,at});at+=p.segments[k].actual;}return out;}
 need(!p.segments.empty(),"single missing sources");std::set<std::size_t>x_indices;std::pair<std::size_t,std::size_t>previous{0,0};
 for(const auto&c:p.public_cells){auto key=std::make_pair(c.source,c.index);need(c.source>0 && c.source<p.segments.size() && c.index<p.segments[c.source].actual && c.x_index<p.segments[0].actual && key>previous && x_indices.insert(c.x_index).second,"single native public coordinate bounds/order/alias");previous=key;}
 need(p.public_cells.empty() || p.public_cells.size()==p.segments[0].actual,"single native public X completeness");
 std::vector<PhysicalRun>runs;std::size_t physical=0,cell=0;
 for(std::size_t k=1;k<p.segments.size();++k){std::size_t begin=0;auto emit=[&](std::size_t end){need(end>=begin,"single public cell order");if(end>begin){runs.push_back({k,begin,end-begin,physical});need(end-begin<=maxDomain-physical,"single compact run overflow");physical+=end-begin;}};
  while(cell<p.public_cells.size() && p.public_cells[cell].source==k){auto index=p.public_cells[cell++].index;emit(index);begin=index+1;}emit(p.segments[k].actual);
 }return runs;
}
void validatePlan(const Plan&p){
 if(p.category_mode){validateCategoryPlan(p);return;}
 need(power(p.domain) && p.domain<=maxDomain && !p.segments.empty() && p.segments.size()<=8193,"single logical domain/segments");
 unified::validate(p.packing);need(p.domain==p.packing.domain && p.segments.size()==p.packing.segments.size(),"single logical packing identity");
 const auto shape=matrixShape(p.physical_values);
 need(p.physical_domain==shape.physical_domain && p.rows==shape.rows && p.columns==shape.columns,"single noncanonical balanced physical shape");
 need(p.source_identity.size()==64 && std::all_of(p.source_identity.begin(),p.source_identity.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}),"single source identity");
 auto runs=deriveRuns(p);need(runs.size()==p.runs.size(),"single compact run count");
 for(std::size_t i=0;i<runs.size();++i){const auto&a=runs[i];const auto&b=p.runs[i];need(a.source==b.source && a.source_offset==b.source_offset && a.count==b.count && a.physical_offset==b.physical_offset,"single noncanonical compact run mapping");}
 std::vector<unified::Segment>intervals;std::size_t actual=0,physical=0;std::set<std::string>ids;
 for(std::size_t k=0;k<p.segments.size();++k){const auto&s=p.segments[k];
  need(s.actual && s.actual<=s.padded && s.actual<=p.domain && actual<=p.domain-s.actual,"single segment actual/overflow");
  need(s.offset==p.packing.segments[k].offset && s.padded==p.packing.segments[k].padded,"single source logical mapping mismatch");
  std::size_t private_count=0;for(const auto&run:runs)if(run.source==k)private_count+=run.count;
  need(s.is_public==(k==0) && s.physical_offset==physical && s.physical_length==private_count,"single noncanonical logical/physical mapping");
  need(!s.source_id.empty() && s.source_id.size()<=160 && ids.insert(s.source_id).second,"single unique source ID");
  need(s.physical_length<=maxDomain-physical,"single physical offset overflow");physical+=s.physical_length;actual+=s.actual;intervals.push_back({s.offset,s.padded});
 }
 need(p.segments[0].offset==0 && p.segments[0].padded<=std::size_t(1)<<24 && actual==p.logical_values && physical==p.physical_values,"single public X/effective lengths");
 unified::validate(p.domain,intervals);
}
void validatePublic(const Public&p){validatePlan(p.plan);need(p.rows.size()==p.plan.rows,"single row commitment count");}
void validateSecret(const Secret&s,const Public&p){
 need(descriptor(s.pc.plan)==descriptor(p.plan) && s.pc.rows==p.rows && s.blinds.size()==p.plan.rows && s.sources.size()==p.plan.segments.size(),"single private commitment identity/shape");
}
Bytes rowBytes(const Public&p,std::size_t start,std::size_t count){Writer w;w.number(count);for(std::size_t j=0;j<count;++j)w.point(p.rows.at(start+j));return w.b;}
// Only actual private data have physical addresses. Logical gaps/source tails
// cannot be supplied by a prover. Physical tail storage is unused by Q.
bool readRow(const Secret&s,std::size_t row,std::vector<Fr>&out){
 const auto&p=s.pc.plan;out.assign(p.columns,Fr(0));auto begin=row*p.columns,end=begin+p.columns;bool touched=false;
 for(const auto&run:p.runs){auto lo=std::max(begin,run.physical_offset),hi=std::min(end,run.physical_offset+run.count);if(lo>=hi)continue;touched=true;
  for(auto j=lo;j<hi;++j){auto f=s.sources[run.source](run.source_offset+j-run.physical_offset);need(f.isValid(),"single invalid private source scalar");out[j-begin]=f;}}
 return touched;
}
G1 rowCombination(const Public&p,const QueryTerm&t){
 need(!t.left.empty() && t.first_row<=p.rows.size() && t.left.size()<=p.rows.size()-t.first_row,"single folded row dimensions");G1 out;out.clear();
 for(std::size_t at=0;at<t.left.size();at+=16384){auto n=std::min<std::size_t>(16384,t.left.size()-at);out+=master3::masterMSM(p.rows.data()+t.first_row+at,t.left.data()+at,n);}return out;
}
struct Fold {hyrax_a3::DotWitness witness;G1 commitment;};
Fold fold(const Public&p,const QueryTerm&t,const Secret*s){
 Fold out;out.commitment=rowCombination(p,t);
 if(s){out.witness.values.assign(p.plan.columns,Fr(0));std::vector<Fr>row;
  for(std::size_t i=0;i<t.left.size();++i){auto index=t.first_row+i;out.witness.blind+=t.left[i]*s->blinds[index];if(t.left[i].isZero() || !readRow(*s,index,row))continue;for(std::size_t j=0;j<row.size();++j)out.witness.values[j]+=t.left[i]*row[j];}}
 return out;
}
struct Basis {
 std::vector<Fr>low,high_low,high_high;std::size_t columns;unsigned split=0;
 Basis(const Plan&p,const std::vector<Fr>&q):columns(p.columns){need(q.size()==bits(p.domain),"single logical query dimension");for(const auto&v:q)need(v.isValid(),"single query scalar");auto d=bits(columns);need(d<=q.size(),"single logical/physical bit dimensions");split=(q.size()-d)/2;low=r1::weights({q.begin(),q.begin()+d});high_low=r1::weights({q.begin()+d,q.begin()+d+split});high_high=r1::weights({q.begin()+d+split,q.end()});}
 Fr highAt(std::size_t j)const{return high_low[j&(high_low.size()-1)]*high_high.at(j>>split);}
 Fr at(std::size_t j)const{return low[j%columns]*highAt(j/columns);}
};
bool nonzero(const std::vector<Fr>&v){return std::any_of(v.begin(),v.end(),[](const Fr&f){return !f.isZero();});}
}
unsigned bits(std::size_t n){need(power(n) && n<=maxDomain,"single metadata dimension");unsigned b=0;while(n>1){n>>=1;++b;}return b;}
PhysicalCommitmentLayout matrixShape(std::size_t actual){
 need(actual && actual<=maxDomain,"single physical actual bound");PhysicalCommitmentLayout out;out.physical_values=actual;out.physical_domain=1;while(out.physical_domain<actual)out.physical_domain*=2;
 auto k=bits(out.physical_domain);out.rows=std::size_t(1)<<(k/2);out.columns=std::size_t(1)<<((k+1)/2);need(out.rows<=maxRows && out.columns<=hyrax_a3::maxDot,"single balanced implementation dimension");return out;
}
Plan makePlan(const unified::Packing&packing,const std::vector<std::size_t>&actual,const std::string&source,const std::vector<std::string>&ids,const std::vector<PublicCell>&native_public){
 unified::validate(packing);need(actual.size()==packing.segments.size() && (ids.empty() || ids.size()==actual.size()),"single source/actual/ID count");Plan p;p.source_identity=source;p.domain=packing.domain;p.packing=packing;
 for(std::size_t k=0;k<actual.size();++k){auto s=packing.segments[k];need(actual[k] && actual[k]<=s.padded && actual[k]<=p.domain && p.logical_values<=p.domain-actual[k],"single logical length overflow");
  p.segments.push_back({s.offset,actual[k],s.padded,p.physical_values,k?actual[k]:0,k==0,ids.empty()?"source/"+std::to_string(k):ids[k]});p.logical_values+=actual[k];if(k){need(actual[k]<=maxDomain-p.physical_values,"single physical length overflow");p.physical_values+=actual[k];}}
 p.public_cells=native_public;std::sort(p.public_cells.begin(),p.public_cells.end(),[](const PublicCell&a,const PublicCell&b){return std::tie(a.source,a.index)<std::tie(b.source,b.index);});p.runs=deriveRuns(p);p.physical_values=0;
 for(std::size_t k=0;k<p.segments.size();++k){auto&s=p.segments[k];s.physical_offset=p.physical_values;s.physical_length=0;for(const auto&run:p.runs)if(run.source==k)s.physical_length+=run.count;p.physical_values+=s.physical_length;}
 static_cast<PhysicalCommitmentLayout&>(p)=matrixShape(p.physical_values);validatePlan(p);return p;
}
Bytes descriptor(const Plan&p){validatePlan(p);Writer w;w.text(suite);w.text(layout);w.text(modelScope);w.text(hyrax_a3::suite);w.text("same-A3-column-bases-and-Pedersen-H;no-trusted-W-bases");w.text(p.source_identity);w.number(p.category_mode);if(p.category_mode)w.text("five-category-tail-only-v1");else w.raw(unified::descriptor(p.packing));
 for(auto n:{p.domain,p.rows,p.columns,p.logical_values,p.physical_values,p.physical_domain})w.number(n,8);
 w.number(p.segments.size());for(const auto&s:p.segments){w.text(s.source_id);for(auto n:{s.offset,s.actual,s.padded,s.physical_offset,s.physical_length})w.number(n,8);w.number(s.is_public);}w.number(p.public_cells.size());for(const auto&c:p.public_cells){w.number(c.source);w.number(c.index,8);w.number(c.x_index,8);}return w.b;
}
void mappedQueryTerms(const Plan&p,const std::vector<Fr>&q,const ConsumeTerm&consume){
 validatePlan(p);need(bool(consume),"single query consumer");Basis basis(p,q);const auto C=p.columns;
 auto emit=[&](QueryTerm&t){if(nonzero(t.left) && nonzero(t.right))consume(t);};
 for(const auto&run:p.runs){auto logical_offset=p.segments[run.source].offset+run.source_offset;auto begin=run.physical_offset,end=begin+run.count;
  auto edge=[&](std::size_t lo,std::size_t hi){QueryTerm t;t.first_row=lo/C;t.left={Fr(1)};t.right.assign(C,Fr(0));for(auto j=lo;j<hi;++j)t.right[j%C]=basis.at(logical_offset+(j-run.physical_offset));emit(t);};
  if(begin%C){auto stop=std::min(end,(begin/C+1)*C);edge(begin,stop);begin=stop;}
  auto full_end=end-end%C;
  if(begin<full_end){auto logical=logical_offset+(begin-run.physical_offset);auto shift=logical%C;auto high=logical/C;
   for(unsigned carry=0;carry<(shift?2u:1u);++carry){QueryTerm t;t.first_row=begin/C;t.left.resize((full_end-begin)/C);t.right.assign(C,Fr(0));
    for(std::size_t i=0;i<t.left.size();++i)t.left[i]=basis.highAt(high+i+carry);
    auto lo=carry?C-shift:0,hi=carry?C:C-shift;for(auto j=lo;j<hi;++j)t.right[j]=basis.low[(j+shift)%C];emit(t);}
   begin=full_end;
  }
  if(begin<end)edge(begin,end);
 }
}
Fr publicContribution(const Plan&p,const std::vector<Fr>&q,const std::vector<Fr>&x){
 validatePlan(p);if(p.category_mode)return Fr(0);need(x.size()==p.segments[0].actual,"single verifier public X shape");Basis basis(p,q);Fr value=0;for(std::size_t j=0;j<x.size();++j){need(x[j].isValid(),"single invalid public X");value+=basis.at(j)*x[j];}for(const auto&c:p.public_cells)value+=basis.at(p.segments[c.source].offset+c.index)*x[c.x_index];return value;
}
Secret commit(const Plan&p,std::vector<Value>sources,Random&rng,unsigned workers){
 validatePlan(p);need(sources.size()==p.segments.size() && workers>0 && workers<=32,"single source/worker bound");for(auto&v:sources)need(bool(v),"single missing sealed source");
 const auto started=std::chrono::steady_clock::now();Secret s;s.pc.plan=p;s.sources=std::move(sources);s.pc.rows.resize(p.rows);s.blinds.reserve(p.rows);for(std::size_t i=0;i<p.rows;++i)s.blinds.push_back(rng.scalar());
 auto bases=hyrax_a3::generators(p.columns);auto H=parameters(1).hiding;workers=std::min<std::size_t>(workers,p.rows);
 std::atomic<std::size_t>next{0};std::atomic<bool>stop{false};std::exception_ptr failure;std::mutex failure_mutex;std::vector<std::uint64_t>calls(workers,0),terms(workers,0);
 auto work=[&](unsigned worker){try{std::vector<Fr>row;std::vector<G1>scratch(std::min<std::size_t>(1024,p.columns));for(;;){auto i=next.fetch_add(1);if(i>=p.rows || stop.load())break;G1 value;value.clear();if(readRow(s,i,row))for(std::size_t j=0;j<p.columns;j+=1024){auto n=std::min<std::size_t>(1024,p.columns-j);std::copy_n(bases.begin()+j,n,scratch.begin());G1 part;G1::mulVec(part,scratch.data(),row.data()+j,n);value+=part;++calls[worker];terms[worker]+=n;}s.pc.rows[i]=value+H*s.blinds[i];}}
 catch(...){stop.store(true);std::lock_guard<std::mutex>lock(failure_mutex);if(!failure)failure=std::current_exception();}};
 std::vector<std::thread>pool;try{for(unsigned j=1;j<workers;++j)pool.emplace_back(work,j);work(0);}catch(...){stop.store(true);for(auto&t:pool)t.join();throw;}for(auto&t:pool)t.join();if(failure)std::rethrow_exception(failure);
 auto&m=metrics();for(unsigned j=0;j<workers;++j){m.msm_calls+=calls[j];m.msm_terms+=terms[j];}m.max_msm=std::max<std::uint64_t>(m.max_msm,std::min<std::size_t>(1024,p.columns));
 std::cerr<<"SINGLE_HYRAX_COMMIT {\"logical_roots\":1,\"rows\":"<<p.rows<<",\"columns\":"<<p.columns<<",\"domain\":"<<p.domain<<",\"physical_domain\":"<<p.physical_domain<<",\"physical_values\":"<<p.physical_values<<",\"logical_values\":"<<p.logical_values<<",\"logical_gap\":"<<p.domain-p.logical_values<<",\"physical_padding\":"<<p.physical_domain-p.physical_values<<",\"workers\":"<<workers<<",\"row_scalar_scratch_bound_bytes\":"<<workers*p.columns*sizeof(Fr)<<",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<"}\n";return s;
}
Bytes encode(const Public&p){validatePublic(p);Writer w;auto header=descriptor(p.plan);w.number(header.size());w.raw(header);auto out=std::move(w.b);out.reserve(out.size()+32*p.rows.size());for(std::size_t i=0;i<p.rows.size();i+=rowChunk){auto chunk=rowBytes(p,i,std::min(rowChunk,p.rows.size()-i));out.insert(out.end(),chunk.begin()+4,chunk.end());}return out;}
Public decode(const Bytes&b,const Plan&expected){auto header=descriptor(expected);Reader r{b};need(r.number()==header.size() && r.raw(header.size())==header,"single untrusted suite/layout/shape");need(b.size()-r.at==expected.rows*32,"single encoded row bytes/EOF");Public p;p.plan=expected;p.rows.reserve(expected.rows);for(std::size_t i=0;i<expected.rows;++i)p.rows.push_back(r.point());r.end();return p;}
std::string identity(const Public&p){validatePublic(p);Writer w;w.raw(descriptor(p.plan));w.number(rowChunk);for(std::size_t i=0;i<p.rows.size();i+=rowChunk){auto digest=r1::hash(rowBytes(p,i,std::min(rowChunk,p.rows.size()-i)));w.raw(Bytes(digest.begin(),digest.end()));}return hex(r1::hash(w.b));}
void writeLayout(std::ostream&out,const Public&p){validatePublic(p);if(p.plan.category_mode){writeCategoryLayout(out,p);return;}out<<"{\"suite\":\""<<suite<<"\",\"layout\":\""<<layout<<"\",\"model_scope\":\""<<modelScope<<"\",\"logical_commitments\":1,\"identity\":\""<<identity(p)<<"\",\"source_layout_identity\":\""<<p.plan.source_identity<<"\",\"rows\":"<<p.plan.rows<<",\"columns\":"<<p.plan.columns<<",\"domain\":"<<p.plan.domain<<",\"logical_values\":"<<p.plan.logical_values<<",\"physical_domain\":"<<p.plan.physical_domain<<",\"physical_values\":"<<p.plan.physical_values<<",\"logical_gap\":"<<p.plan.domain-p.plan.logical_values<<",\"physical_padding\":"<<p.plan.physical_domain-p.plan.physical_values<<",\"fixed_zero_sigma_rows\":0,\"source_segments\":[";
 for(std::size_t k=0;k<p.plan.segments.size();++k){if(k)out<<',';const auto&s=p.plan.segments[k];out<<"{\"storage_index\":"<<k<<",\"source_id\":\""<<s.source_id<<"\",\"offset\":"<<s.offset<<",\"logical_length\":"<<s.padded<<",\"actual\":"<<s.actual<<",\"padded\":"<<s.padded<<",\"physical_offset\":"<<s.physical_offset<<",\"physical_length\":"<<s.physical_length<<",\"public\":"<<(s.is_public?"true":"false")<<",\"witness_source\":\""<<(s.is_public?"verifier-statement":"sealed-native-source")<<"\"}";}
 out<<"],\"private_runs\":[";bool first=true;for(const auto&r:p.plan.runs){if(!first)out<<',';first=false;out<<"{\"source\":"<<r.source<<",\"source_offset\":"<<r.source_offset<<",\"logical_offset\":"<<p.plan.segments[r.source].offset+r.source_offset<<",\"count\":"<<r.count<<",\"physical_offset\":"<<r.physical_offset<<'}';}
 out<<"],\"native_public_cells\":[";first=true;for(const auto&c:p.plan.public_cells){if(!first)out<<',';first=false;out<<"["<<c.source<<','<<c.index<<','<<c.x_index<<"]";}out<<"],\"logical_layout\":";unified::writeLayout(out,p.plan.packing);out<<"}\n";
}
Public exchange(IO&io,const Plan&plan,const Secret*s){
 auto header=descriptor(plan);need(io.prover()?s!=nullptr:s==nullptr,"single PC P/V boundary");if(s){validatePublic(s->pc);need(descriptor(s->pc.plan)==header,"single PC secret plan");}
 bindPublic(io,"SingleHyrax/header",header);Public p;p.plan=plan;p.rows.reserve(plan.rows);
 for(std::size_t i=0;i<plan.rows;i+=rowChunk){auto n=std::min(rowChunk,plan.rows-i);auto b=io.message("SingleHyrax/rows/"+std::to_string(i),s?rowBytes(s->pc,i,n):Bytes{});Reader r{b};need(r.number()==n,"single row frame count");for(std::size_t j=0;j<n;++j)p.rows.push_back(r.point());r.end();}
 Writer seal;seal.text(suite);seal.text(identity(p));seal.text(io.session());bindPublic(io,"SingleHyrax/sealed",seal.b);return p;
}
Fr evaluate(const Secret&s,const std::vector<Fr>&q){
 validatePublic(s.pc);Basis basis(s.pc.plan,q);std::vector<Fr>x;if(!s.pc.plan.category_mode){x.resize(s.pc.plan.segments[0].actual);for(std::size_t j=0;j<x.size();++j)x[j]=s.sources[0](j);}Fr result=s.pc.plan.category_mode?Fr(0):publicContribution(s.pc.plan,q,x);
 for(const auto&run:s.pc.plan.runs){for(std::size_t j=0;j<run.count;++j){result+=basis.at(s.pc.plan.segments[run.source].offset+run.source_offset+j)*s.sources[run.source](run.source_offset+j);}}
 return result;
}
void open(IO&io,const Public&p,const Query&q,const std::vector<Fr>&x,const Secret*s,const Hidden*h,Random*rng,const std::string&role){
 validatePublic(p);need(io.prover()?(s && h && rng):(!s && !h && !rng),"single opening P/V boundary");need(!role.empty() && role.size()<=120,"single opening role bound");crypto::validate(q.evaluation,io.session(),q.evaluation.role);
 if(s){validateSecret(*s,p);need(crypto::encode(h->handle)==crypto::encode(q.evaluation),"single hidden opening handle");}
 const auto started=std::chrono::steady_clock::now();auto id=identity(p);Writer binding;binding.text(suite);binding.text(id);binding.text(io.session());binding.text(role);binding.raw(crypto::encode(q.evaluation));for(const auto&v:q.point)binding.scalar(v);binding.number(x.size(),8);bindPublic(io,role+"/binding",binding.b);
 for(std::size_t start=0;!p.plan.category_mode && start<x.size();start+=65536){Writer chunk;chunk.number(start,8);auto n=std::min<std::size_t>(65536,x.size()-start);chunk.number(n);for(std::size_t j=0;j<n;++j)chunk.scalar(x[start+j]);bindPublic(io,role+"/public-X/"+std::to_string(start),chunk.b);}
 std::vector<Hidden>evaluations;std::size_t index=0;double preparation=0;
 mappedQueryTerms(p.plan,q.point,[&](const QueryTerm&t){auto begin=std::chrono::steady_clock::now();auto f=fold(p,t,s);preparation+=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();auto name=role+"/term/"+std::to_string(index++);auto y=exchangeHidden(io,name+"/evaluation",s?r1::dot(f.witness.values,t.right):Fr(0),rng);
  if(s){f.witness.evaluation=y;}
  stage2::ScopedIO scoped(io,name);hyrax_a3::dotProduct(scoped,f.commitment,y.handle,t.right,s?&f.witness:nullptr,rng);evaluations.push_back(y);
 });
 if(!p.plan.category_mode){evaluations.push_back(publicHidden(io,role+"/public-contribution",publicContribution(p.plan,q.point,x)));}
 auto total=combine(io.session(),role+"/stitched",evaluations,std::vector<Fr>(evaluations.size(),Fr(1)));bindPublic(io,role+"/stitched",crypto::encode(total.handle));Hidden expected;if(h)expected=*h;else expected.handle=q.evaluation;
 std::vector<Hidden>w{total,expected};Relations equality;equality.role=role+"/logical-consistency";for(auto&v:w)equality.handles.push_back(v.handle);equality.linear={{Fr(1),Fr(-1)}};sigma(io,equality,s?&w:nullptr,rng);
 std::cerr<<"COMPACT_QUERY {\"rank_terms\":"<<index<<",\"logical_domain\":"<<p.plan.domain<<",\"physical_domain\":"<<p.plan.physical_domain<<",\"preparation_seconds\":"<<preparation<<",\"total_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<",\"logical_coefficient_array_bytes\":0}\n";
}
Hidden evaluateAndOpen(IO&io,const Public&p,const std::vector<Fr>&q,const std::vector<Fr>&x,const Secret*s,Random*rng,const std::string&role){
 need(io.prover()?(s && rng):(!s && !rng),"single evaluation P/V boundary");auto h=exchangeHidden(io,role+"/evaluation",s?evaluate(*s,q):Fr(0),rng);open(io,p,{q,h.handle},x,s,s?&h:nullptr,rng,role);return h;
}
}
