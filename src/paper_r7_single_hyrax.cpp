#include "paper_r7_single_hyrax.hpp"
#include "paper_r7_streaming_master.hpp"
#include "paper_r7_stage2_opening.hpp"
#include "paper_r7_unified_workers.hpp"
#include "paper_r7_native_workers.hpp"
#include <algorithm>
#include <atomic>
#include <exception>
#include <mutex>
#include <thread>
#include <chrono>
#include <iostream>
#include <set>
#include <tuple>
#include <cybozu/sha2.hpp>
namespace zkgpt::r7::oneblock::single {
namespace {
constexpr std::size_t rowChunk=256;
bool power(std::size_t n){return n && !(n&(n-1));}
std::vector<PhysicalRun>deriveRuns(const Plan&p){
 if(p.category_mode){need(p.public_cells.empty(),"category X cannot have public holes");std::vector<PhysicalRun>out;for(std::size_t k=0;k<p.segments.size();++k){out.push_back({k,0,p.segments[k].actual,p.segments[k].offset});}return out;}
 need(!p.segments.empty(),"single missing sources");std::set<std::size_t>x_indices;std::pair<std::size_t,std::size_t>previous{0,0};
 for(const auto&c:p.public_cells){auto key=std::make_pair(c.source,c.index);need(c.source>0 && c.source<p.segments.size() && c.index<p.segments[c.source].actual && c.x_index<p.segments[0].actual && key>previous && x_indices.insert(c.x_index).second,"single native public coordinate bounds/order/alias");previous=key;}
 need(p.public_cells.empty() || p.public_cells.size()==p.segments[0].actual,"single native public X completeness");
 std::vector<PhysicalRun>runs;std::size_t cell=0;
 for(std::size_t k=1;k<p.segments.size();++k){std::size_t begin=0;auto emit=[&](std::size_t end){need(end>=begin,"single public cell order");if(end>begin){runs.push_back({k,begin,end-begin,p.segments[k].offset+begin});}};
  while(cell<p.public_cells.size() && p.public_cells[cell].source==k){auto index=p.public_cells[cell++].index;emit(index);begin=index+1;}emit(p.segments[k].actual);
 }return runs;
}
void validatePlan(const Plan&p){
 if(p.category_mode){validateCategoryPlan(p);return;}
 need(power(p.domain) && p.domain<=maxDomain && !p.segments.empty() && p.segments.size()<=8193,"single logical domain/segments");
 need(p.packing && p.domain==p.packing.domain() && p.segments.size()==p.packing.segments().size(),"single logical packing identity");
 const auto shape=matrixShape(p.domain);
 need(p.physical_domain==shape.physical_domain && p.rows==shape.rows && p.columns==shape.columns,"single noncanonical balanced physical shape");
 need(p.source_identity.size()==64 && std::all_of(p.source_identity.begin(),p.source_identity.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}),"single source identity");
 auto runs=deriveRuns(p);need(runs.size()==p.runs.size(),"single source run count");
 for(std::size_t i=0;i<runs.size();++i){const auto&a=runs[i];const auto&b=p.runs[i];need(a.source==b.source && a.source_offset==b.source_offset && a.count==b.count && a.physical_offset==b.physical_offset,"single noncanonical direct source run");}
 std::vector<unified::Segment>intervals;std::size_t actual=0,physical=0;std::set<std::string>ids;
 for(std::size_t k=0;k<p.segments.size();++k){const auto&s=p.segments[k];
  need(s.actual && s.actual<=s.padded && s.actual<=p.domain && actual<=p.domain-s.actual,"single segment actual/overflow");
  need(s.offset==p.packing.segments()[k].offset && s.padded==p.packing.segments()[k].padded,"single source logical mapping mismatch");
  std::size_t private_count=0;for(const auto&run:runs)if(run.source==k)private_count+=run.count;
  need(s.is_public==(k==0) && s.physical_offset==s.offset && s.physical_length==private_count,"single noncanonical logical/physical mapping");
  need(!s.source_id.empty() && s.source_id.size()<=160 && ids.insert(s.source_id).second,"single unique source ID");
  need(s.physical_length<=maxDomain-physical,"single physical offset overflow");physical+=s.physical_length;actual+=s.actual;intervals.push_back({s.offset,s.padded});
 }
 need(p.segments[0].offset==0 && p.segments[0].padded<=std::size_t(1)<<24 && actual==p.logical_values && physical==p.physical_values,"single public X/effective lengths");
 unified::validate(p.domain,intervals);
}
Bytes rowBytes(const Public&p,std::size_t start,std::size_t count){Writer w;w.number(count);for(std::size_t j=0;j<count;++j)w.point(p.rows().at(start+j));return w.b;}
using Clock=std::chrono::steady_clock;
double seconds(Clock::time_point start){return std::chrono::duration<double>(Clock::now()-start).count();}
using Runs=std::vector<PhysicalRun>;
Runs sortedRuns(const Plan&p){
 auto runs=p.runs;
 std::sort(runs.begin(),runs.end(),[](const auto&a,const auto&b){return a.physical_offset<b.physical_offset;});
 return runs;
}
// Read only occupied coordinates, preserving their original column indices.
// A null window points to a source that still needs its scalar accessor.
template<class F>void visitRow(const Secret&s,const Runs&runs,std::size_t row,F&&consume){
 const auto columns=s.pc.plan().columns;
 const auto begin=row*columns,end=begin+columns;
 auto it=std::lower_bound(runs.begin(),runs.end(),begin,[](const auto&run,std::size_t at){return run.physical_offset+run.count<=at;});
 for(;it!=runs.end() && it->physical_offset<end;++it){
  auto at=std::max(begin,it->physical_offset);
  const auto last=std::min(end,it->physical_offset+it->count);
  auto source=it->source_offset+at-it->physical_offset;
  const auto&window=s.windows[it->source];
  while(at<last){
   auto part=window?window(source,last-at):unified::InputWindow{nullptr,last-at};
   need(part.count && part.count<=last-at,"single sealed window bound");
   consume(at-begin,source,part,s.sources[it->source]);
   at+=part.count;
   source+=part.count;
  }
 }
}
struct alignas(64) RowStats {
 std::uint64_t calls=0,terms=0,max_terms=0,empty=0,full=0,direct=0,copied=0,callbacks=0,compacted=0,base_copies=0;
 double msm_seconds=0;
};
G1 commitRow(const Secret&s,const Runs&runs,std::size_t row,const G1*bases,std::vector<Fr>&scratch,std::vector<G1>&compact,RowStats&stats){
 std::size_t first=0,count=0,last=0;
 const Fr*direct=nullptr;
 compact.clear();
 visitRow(s,runs,row,[&](std::size_t column,std::size_t source,unified::InputWindow part,const Value&read){
  if(!count){
   first=column;
   if(part.data){
    direct=part.data;
    count=part.count;
    last=column+part.count;
    stats.direct+=part.count;
    return;
   }
  }else if(column!=last && compact.empty()){
   // Disjoint public extents use a single MSM over the matching bases.
   // A full row never copies its bases, including rows crossing windows.
   compact.reserve(s.pc.plan().columns);
   compact.insert(compact.end(),bases+first,bases+last);
  }
  if(!compact.empty())compact.insert(compact.end(),bases+column,bases+column+part.count);
  if(scratch.empty())scratch.resize(s.pc.plan().columns);
  if(direct){
   std::copy_n(direct,count,scratch.data());
   stats.copied+=count;
   direct=nullptr;
  }
  if(part.data){
   std::copy_n(part.data,part.count,scratch.data()+count);
   stats.direct+=part.count;
   stats.copied+=part.count;
  }else{
   for(std::size_t j=0;j<part.count;++j)scratch[count+j]=read(source+j);
   stats.callbacks+=part.count;
  }
  count+=part.count;
  last=column+part.count;
 });
 G1 result;
 if(!count){++stats.empty;result.clear();return result;}
 if(!compact.empty()){
  ++stats.compacted;
  stats.base_copies+=compact.size();
 }
 const auto start=Clock::now();
 // The bundled MCL mulVec's GLV and fallback routines borrow these inputs.
 // GLV normalizes private tables and the scalar input is const.
 G1::mulVec(result,compact.empty()?const_cast<G1*>(bases+first):compact.data(),direct?direct:scratch.data(),count);
 stats.msm_seconds+=seconds(start);
 if(count==s.pc.plan().columns)++stats.full;
 ++stats.calls;stats.terms+=count;stats.max_terms=std::max<std::uint64_t>(stats.max_terms,count);
 return result;
}
G1 rowCombination(const Public&p,const QueryTerm&t){
 need(!t.left.empty() && t.first_row<=p.rows().size() && t.left.size()<=p.rows().size()-t.first_row,"single folded row dimensions");G1 out;out.clear();
 for(std::size_t at=0;at<t.left.size();at+=16384){auto n=std::min<std::size_t>(16384,t.left.size()-at);out+=master3::masterMSM(p.rows().data()+t.first_row+at,t.left.data()+at,n);}return out;
}
struct alignas(64) FoldPart {
 hyrax_a3::DotWitness witness;
 std::uint64_t direct=0,callbacks=0;
};
hyrax_a3::DotWitness foldRows(const Secret&s,const QueryTerm&t,unsigned requested,std::ostream*log){
 const auto&plan=s.pc.plan();
 need(t.first_row<=plan.rows && !t.left.empty() && t.left.size()<=plan.rows-t.first_row && s.blinds.size()==plan.rows,"single witness fold shape");
 const auto started=Clock::now();
 const auto runs=sortedRuns(plan);
 const auto workers=t.left.size()*plan.columns>=65536?std::min<std::size_t>({16,requested,t.left.size()}):1;
 std::vector<FoldPart>partials(workers);
 auto job=[&](std::size_t worker){
  auto&part=partials[worker];
  auto&out=part.witness;
  out.values.assign(plan.columns,Fr(0));
  for(auto i=t.left.size()*worker/workers;i<t.left.size()*(worker+1)/workers;++i){
   const auto index=t.first_row+i;
   const auto&weight=t.left[i];
   out.blind+=weight*s.blinds[index];
   if(weight.isZero())continue;
   visitRow(s,runs,index,[&](std::size_t column,std::size_t source,unified::InputWindow input,const Value&read){
    auto*dest=out.values.data()+column;
    if(input.data){
     for(std::size_t j=0;j<input.count;++j)dest[j]+=weight*input.data[j];
     part.direct+=input.count;
    }else{
     for(std::size_t j=0;j<input.count;++j)dest[j]+=weight*read(source+j);
     part.callbacks+=input.count;
    }
   });
  }
 };
 {
  unified::detail::Workers pool(workers);
  pool.run(workers,job,workers>1);
 }
 const auto reduction=Clock::now();
 auto result=std::move(partials[0].witness);
 std::uint64_t direct=partials[0].direct,callbacks=partials[0].callbacks;
 for(std::size_t w=1;w<workers;++w){
  const auto&part=partials[w];
  result.blind+=part.witness.blind;
  for(std::size_t j=0;j<plan.columns;++j)result.values[j]+=part.witness.values[j];
  direct+=part.direct;
  callbacks+=part.callbacks;
 }
 const auto reduction_seconds=seconds(reduction);
 if(log)*log<<"SINGLE_HYRAX_FOLD {\"workers\":"<<workers<<",\"rows\":"<<t.left.size()<<",\"columns\":"<<plan.columns
            <<",\"direct_scalars\":"<<direct<<",\"callback_scalars\":"<<callbacks
            <<",\"local_vector_bytes\":"<<workers*plan.columns*sizeof(Fr)<<",\"reduction_seconds\":"<<reduction_seconds
            <<",\"seconds\":"<<seconds(started)<<"}\n";
 return result;
}
struct Fold {hyrax_a3::DotWitness witness;G1 commitment;};
Fold fold(const Public&p,const QueryTerm&t,const Secret*s){
 Fold out;
 const auto start=Clock::now();
 out.commitment=rowCombination(p,t);
 std::cerr<<"SINGLE_HYRAX_ROW_COMBINATION {\"rows\":"<<t.left.size()<<",\"seconds\":"<<seconds(start)<<"}\n";
 if(s)out.witness=foldRows(*s,t,s->workers,&std::cerr);
 return out;
}
struct Basis {
 std::vector<Fr>low,high_low,high_high;std::size_t columns;unsigned split=0;
 Basis(const Plan&p,const std::vector<Fr>&q):columns(p.columns){need(q.size()==bits(p.domain),"single logical query dimension");auto d=bits(columns);need(d<=q.size(),"single logical/physical bit dimensions");split=(q.size()-d)/2;low=r1::weights({q.begin(),q.begin()+d});high_low=r1::weights({q.begin()+d,q.begin()+d+split});high_high=r1::weights({q.begin()+d+split,q.end()});}
 Fr highAt(std::size_t j)const{return high_low[j&(high_low.size()-1)]*high_high.at(j>>split);}
 Fr at(std::size_t j)const{return low[j%columns]*highAt(j/columns);}
};

}
unsigned bits(std::size_t n){need(power(n) && n<=maxDomain,"single metadata dimension");unsigned b=0;while(n>1){n>>=1;++b;}return b;}
PhysicalCommitmentLayout matrixShape(std::size_t actual){
 need(actual && actual<=maxDomain,"single physical actual bound");PhysicalCommitmentLayout out;out.physical_values=actual;out.physical_domain=1;while(out.physical_domain<actual)out.physical_domain*=2;
 auto k=bits(out.physical_domain);out.rows=std::size_t(1)<<(k/2);out.columns=std::size_t(1)<<((k+1)/2);need(out.rows<=maxRows && out.columns<=hyrax_a3::maxDot,"single balanced implementation dimension");return out;
}
Plan makePlan(const unified::Packing&packing,const std::vector<std::size_t>&actual,const std::string&source,const std::vector<std::string>&ids,const std::vector<PublicCell>&native_public){
 need(packing && actual.size()==packing.segments().size() && (ids.empty() || ids.size()==actual.size()),"single source/actual/ID count");Plan p;p.source_identity=source;p.domain=packing.domain();p.packing=packing;
 for(std::size_t k=0;k<actual.size();++k){auto s=packing.segments()[k];need(actual[k] && actual[k]<=s.padded && actual[k]<=p.domain && p.logical_values<=p.domain-actual[k],"single logical length overflow");
  p.segments.push_back({s.offset,actual[k],s.padded,s.offset,k?actual[k]:0,k==0,ids.empty()?"source/"+std::to_string(k):ids[k]});p.logical_values+=actual[k];if(k){need(actual[k]<=maxDomain-p.physical_values,"single physical length overflow");p.physical_values+=actual[k];}}
 p.public_cells=native_public;std::sort(p.public_cells.begin(),p.public_cells.end(),[](const PublicCell&a,const PublicCell&b){return std::tie(a.source,a.index)<std::tie(b.source,b.index);});p.runs=deriveRuns(p);p.physical_values=0;
 for(std::size_t k=0;k<p.segments.size();++k){auto&s=p.segments[k];s.physical_offset=s.offset;s.physical_length=0;for(const auto&run:p.runs)if(run.source==k)s.physical_length+=run.count;p.physical_values+=s.physical_length;}
 static_cast<PhysicalCommitmentLayout&>(p)=matrixShape(p.domain);p.physical_values=0;for(const auto&run:p.runs)p.physical_values+=run.count;validatePlan(p);return p;
}
Bytes descriptor(const Plan&p){Writer w;w.text(suite);w.text(layout);w.text(modelScope);w.text(hyrax_a3::suite);w.text("same-A3-column-bases-and-Pedersen-H;no-trusted-W-bases");w.text(p.source_identity);w.number(p.category_mode);if(p.category_mode)w.text("five-category-tail-only-v1");else w.raw(unified::descriptor(p.packing));
 for(auto n:{p.domain,p.rows,p.columns,p.logical_values,p.physical_values,p.physical_domain})w.number(n,8);
 w.number(p.segments.size());for(const auto&s:p.segments){w.text(s.source_id);for(auto n:{s.offset,s.actual,s.padded,s.physical_offset,s.physical_length})w.number(n,8);w.number(s.is_public);}w.number(p.public_cells.size());for(const auto&c:p.public_cells){w.number(c.source);w.number(c.index,8);w.number(c.x_index,8);}return w.b;
}
void mappedQueryTerms(const Plan&p,const std::vector<Fr>&q,const ConsumeTerm&consume){
 need(bool(consume),"single query consumer");Basis basis(p,q);
 QueryTerm term;term.first_row=0;term.left.resize(p.rows);term.right=basis.low;
 for(std::size_t i=0;i<p.rows;++i)term.left[i]=basis.highAt(i);
 consume(term);
}

Fr publicContribution(const Plan&p,const std::vector<Fr>&q,const std::vector<Fr>&x){
 if(p.category_mode)return Fr(0);
 need(x.size()==p.segments[0].actual,"single verifier public X shape");Basis basis(p,q);Fr value=0;for(std::size_t j=0;j<x.size();++j)value+=basis.at(j)*x[j];for(const auto&c:p.public_cells)value+=basis.at(p.segments[c.source].offset+c.index)*x[c.x_index];return value;
}
hyrax_a3::DotWitness foldWitness(const Secret&s,const QueryTerm&t,unsigned workers){
 need(workers>0 && workers<=32,"single witness fold worker bound");
 return foldRows(s,t,workers,nullptr);
}
Secret commit(const Plan&p,std::vector<Value>sources,Random&rng,unsigned workers){return commit(p,std::move(sources),rng,workers,{});}
Secret commit(const Plan&p,std::vector<Value>sources,Random&rng,unsigned workers,std::vector<SourceWindow>windows){
 validatePlan(p);
 need(sources.size()==p.segments.size() && workers>0 && workers<=32,"single source/worker bound");for(auto&v:sources)need(bool(v),"single missing sealed source");
 const auto started=Clock::now();
 const auto runs=sortedRuns(p);
 if(windows.empty())windows.resize(sources.size());
 need(windows.size()==sources.size(),"single source window count");
 Secret s;s.pc.plan_=p;s.sources=std::move(sources);s.windows=std::move(windows);s.pc.rows_.resize(p.rows);
 s.workers=workers;
 const auto blind_start=Clock::now();
 s.blinds.reserve(p.rows);for(std::size_t i=0;i<p.rows;++i)s.blinds.push_back(rng.scalar());
 const auto blind_seconds=seconds(blind_start);
 const auto base_start=Clock::now();
 const auto bases=hyrax_a3::generatorView(p.columns);
 const auto H=parameters(1).hiding;
 const auto base_seconds=seconds(base_start);
 workers=std::min<std::size_t>(workers,p.rows);
 std::atomic<std::size_t>next{0};std::atomic<bool>stop{false};std::exception_ptr failure;std::mutex failure_mutex;std::vector<RowStats>stats(workers);
 const auto worker_start=Clock::now();
 auto work=[&](unsigned worker){try{
  std::vector<Fr>scratch;std::vector<G1>compact;
  for(;;){
   auto i=next.fetch_add(1);
   if(i>=p.rows || stop.load())break;
   auto value=commitRow(s,runs,i,bases.data(),scratch,compact,stats[worker]);
   s.pc.rows_[i]=value+H*s.blinds[i];
  }
 }catch(...){stop.store(true);std::lock_guard<std::mutex>lock(failure_mutex);if(!failure)failure=std::current_exception();}};
 std::vector<std::thread>pool;try{for(unsigned j=1;j<workers;++j)pool.emplace_back(work,j);work(0);}catch(...){stop.store(true);for(auto&t:pool)t.join();throw;}for(auto&t:pool)t.join();if(failure)std::rethrow_exception(failure);
 const auto worker_seconds=seconds(worker_start);
 RowStats total;
 for(const auto&stat:stats){
  total.calls+=stat.calls;total.terms+=stat.terms;total.max_terms=std::max(total.max_terms,stat.max_terms);
  total.empty+=stat.empty;total.full+=stat.full;total.direct+=stat.direct;total.copied+=stat.copied;total.callbacks+=stat.callbacks;total.msm_seconds+=stat.msm_seconds;
  total.compacted+=stat.compacted;total.base_copies+=stat.base_copies;
 }
 auto&m=metrics();m.msm_calls+=total.calls;m.msm_terms+=total.terms;m.max_msm=std::max(m.max_msm,total.max_terms);
 std::cerr<<"SINGLE_HYRAX_COMMIT {\"logical_roots\":1,\"rows\":"<<p.rows<<",\"columns\":"<<p.columns<<",\"domain\":"<<p.domain
  <<",\"physical_domain\":"<<p.physical_domain<<",\"physical_values\":"<<p.physical_values<<",\"logical_values\":"<<p.logical_values
  <<",\"logical_gap\":"<<p.domain-p.logical_values<<",\"physical_padding\":"<<p.physical_domain-p.physical_values<<",\"workers\":"<<workers
  <<",\"msm_calls\":"<<total.calls<<",\"msm_terms\":"<<total.terms<<",\"empty_rows\":"<<total.empty<<",\"full_rows\":"<<total.full<<",\"partial_rows\":"<<total.calls-total.full
  <<",\"public_padding_skipped\":"<<p.physical_domain-total.terms<<",\"callback_scalars\":"<<total.callbacks
  <<",\"direct_scalars\":"<<total.direct<<",\"copied_scalar_bytes\":"<<total.copied*sizeof(Fr)
  <<",\"compacted_rows\":"<<total.compacted<<",\"copied_generator_bytes\":"<<total.base_copies*sizeof(G1)
  <<",\"row_scalar_scratch_bound_bytes\":"<<workers*p.columns*sizeof(Fr)
  <<",\"worker_wall_seconds\":"<<worker_seconds<<",\"msm_worker_seconds\":"<<total.msm_seconds
  <<",\"blind_rng_seconds\":"<<blind_seconds<<",\"generator_seconds\":"<<base_seconds<<",\"seconds\":"<<seconds(started)<<"}\n";
 return s;
}
Bytes encode(const Public&p){need(p.rows().size()==p.plan().rows,"single row commitment count");Writer w;auto header=descriptor(p.plan());w.number(header.size());w.raw(header);auto out=std::move(w.b);out.reserve(out.size()+32*p.rows().size());for(std::size_t i=0;i<p.rows().size();i+=rowChunk){auto chunk=rowBytes(p,i,std::min(rowChunk,p.rows().size()-i));out.insert(out.end(),chunk.begin()+4,chunk.end());}return out;}
Public decode(const Bytes&b,const Plan&expected){validatePlan(expected);auto header=descriptor(expected);Reader r{b};need(r.number()==header.size() && r.raw(header.size())==header,"single untrusted suite/layout/shape");need(b.size()-r.at==expected.rows*32,"single encoded row bytes/EOF");std::vector<G1>rows;rows.reserve(expected.rows);for(std::size_t i=0;i<expected.rows;++i)rows.push_back(r.point());r.end();return Public(expected,std::move(rows));}
std::string identity(const Public&p){
 const auto started=std::chrono::steady_clock::now();
 auto encoded=encode(p);
 need(encoded.size()>=4,"single identity encoding length");
 Digest digest;cybozu::Sha256 hash;
 need(hash.digest(digest.data(),digest.size(),encoded.data()+4,encoded.size()-4)==digest.size(),"single identity SHA256");
 std::cerr<<"SINGLE_HYRAX_IDENTITY {\"encoded_bytes\":"<<encoded.size()<<",\"buffer_capacity_bytes\":"<<encoded.capacity()<<",\"sha256_calls\":1,\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<"}\n";
 return hex(digest);
}
Public exchange(IO&io,const Plan&plan,const Secret*s){
 auto header=descriptor(plan);need(io.prover()?s!=nullptr:s==nullptr,"single PC P/V boundary");if(s)need(s->pc.rows().size()==plan.rows,"single PC row count");
 bindPublic(io,"SingleHyrax/header",header);Public p;p.plan_=plan;p.rows_.reserve(plan.rows);
 for(std::size_t i=0;i<plan.rows;i+=rowChunk){
  auto n=std::min(rowChunk,plan.rows-i);
  auto label="SingleHyrax/rows/"+std::to_string(i);
  if(s){
   io.message(label,rowBytes(s->pc,i,n));
   p.rows_.insert(p.rows_.end(),s->pc.rows().begin()+i,s->pc.rows().begin()+i+n);
  }else{
   auto b=io.message(label);Reader r{b};need(r.number()==n,"single row frame count");
   for(std::size_t j=0;j<n;++j)p.rows_.push_back(r.point());
   r.end();
  }
 }
 Writer seal;seal.text(suite);seal.text(identity(p));seal.text(io.session());bindPublic(io,"SingleHyrax/sealed",seal.b);p.sealed_transcript_=io.transcriptOwner();p.sealed_session_=io.session();return p;
}
Fr evaluate(const Secret&s,const std::vector<Fr>&q){
 Basis basis(s.pc.plan(),q);std::vector<Fr>x;if(!s.pc.plan().category_mode){x.resize(s.pc.plan().segments[0].actual);for(std::size_t j=0;j<x.size();++j)x[j]=s.sources[0](j);}Fr result=s.pc.plan().category_mode?Fr(0):publicContribution(s.pc.plan(),q,x);
 for(const auto&run:s.pc.plan().runs){for(std::size_t j=0;j<run.count;++j){result+=basis.at(s.pc.plan().segments[run.source].offset+run.source_offset+j)*s.sources[run.source](run.source_offset+j);}}
 return result;
}
void open(IO&io,const Public&p,const Query&q,const std::vector<Fr>&x,const Secret*s,const Hidden*h,Random*rng,const std::string&role){
 need(p.sealed_transcript_.lock()==io.transcriptOwner() && p.sealed_session_==io.session(),"single opening requires exchange on same live transcript");
 need(io.prover()?(s && h && rng):(!s && !h && !rng),"single opening P/V boundary");need(!role.empty() && role.size()<=120,"single opening role bound");crypto::validate(q.evaluation,io.session(),q.evaluation.role);
 if(s)need(s->blinds.size()==p.plan().rows && s->sources.size()==p.plan().segments.size(),"single private commitment shape");
 const auto started=std::chrono::steady_clock::now();Writer binding;binding.text(suite);binding.text(io.session());binding.text(role);binding.raw(crypto::encode(q.evaluation));for(const auto&v:q.point)binding.scalar(v);binding.number(x.size(),8);bindPublic(io,role+"/binding",binding.b);
 for(std::size_t start=0;!p.plan().category_mode && start<x.size();start+=65536){Writer chunk;chunk.number(start,8);auto n=std::min<std::size_t>(65536,x.size()-start);chunk.number(n);for(std::size_t j=0;j<n;++j)chunk.scalar(x[start+j]);bindPublic(io,role+"/public-X/"+std::to_string(start),chunk.b);}
 std::vector<Hidden>evaluations;std::size_t index=0;double preparation=0;const auto calls_before=hyrax_a3::measurement().dot_product_calls;
 mappedQueryTerms(p.plan(),q.point,[&](const QueryTerm&t){auto begin=std::chrono::steady_clock::now();auto f=fold(p,t,s);preparation+=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();auto name=role+"/term/"+std::to_string(index++);auto y=exchangeHidden(io,name+"/evaluation",s?r1::dot(f.witness.values,t.right):Fr(0),rng);
  if(s){f.witness.evaluation=y;}
  auto* workers=s && t.right.size()>=512?&detail::nativeWorkers():nullptr;
  stage2::ScopedIO scoped(io,name);hyrax_a3::dotProduct(scoped,f.commitment,y.handle,t.right,s?&f.witness:nullptr,rng,workers);evaluations.push_back(y);
 });
 if(!p.plan().category_mode){evaluations.push_back(publicHidden(io,role+"/public-contribution",publicContribution(p.plan(),q.point,x)));}
 auto total=combine(io.session(),role+"/stitched",evaluations,std::vector<Fr>(evaluations.size(),Fr(1)));bindPublic(io,role+"/stitched",crypto::encode(total.handle));Hidden expected;if(h)expected=*h;else expected.handle=q.evaluation;
 std::vector<Hidden>w{total,expected};Relations equality;equality.role=role+"/logical-consistency";for(auto&v:w)equality.handles.push_back(v.handle);equality.linear={{Fr(1),Fr(-1)}};sigma(io,equality,s?&w:nullptr,rng);
 const auto a3_calls=hyrax_a3::measurement().dot_product_calls-calls_before;
 std::cerr<<"COMPACT_QUERY {\"rank_terms\":"<<index<<",\"A3_IPA_calls\":"<<a3_calls<<",\"logical_domain\":"<<p.plan().domain<<",\"physical_domain\":"<<p.plan().physical_domain<<",\"preparation_seconds\":"<<preparation<<",\"total_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<",\"logical_coefficient_array_bytes\":0}\n";
}
Hidden evaluateAndOpen(IO&io,const Public&p,const std::vector<Fr>&q,const std::vector<Fr>&x,const Secret*s,Random*rng,const std::string&role){
 need(p.sealed_transcript_.lock()==io.transcriptOwner() && p.sealed_session_==io.session(),"single evaluation requires exchange on same live transcript");
 need(io.prover()?(s && rng):(!s && !rng),"single evaluation P/V boundary");auto h=exchangeHidden(io,role+"/evaluation",s?evaluate(*s,q):Fr(0),rng);open(io,p,{q,h.handle},x,s,s?&h:nullptr,rng,role);return h;
}
}
