#include "paper_r7_classified_commitments.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <sched.h>
namespace zkgpt::r7::oneblock::classified {
namespace {
unsigned defaultCommitWorkers(){
 cpu_set_t allowed;
 CPU_ZERO(&allowed);
 if(sched_getaffinity(0,sizeof(allowed),&allowed)!=0)return 4;
 return unsigned(std::clamp(CPU_COUNT(&allowed),1,32));
}
struct Borrow {
 std::size_t begin,count;
 Value value;
 const std::vector<Fr>*storage=nullptr;
 const Fr*identity=nullptr;
 std::size_t size=0,offset=0;
 unified::InputWindow bounded(std::size_t within,std::size_t length)const {
  need(within<count,"category sealed source piece bounds");
  length=std::min(length,count-within);
  if(!storage)return {nullptr,length};
  need(offset<=size && count<=size-offset,"category sealed source backing extent");
  need(storage->size()==size && storage->data()==identity,"category sealed source storage changed");
  return {identity+offset+within,length};
 }
};
}
void Commitments::initializeCategory(Value base,Prepared*prepared,const std::vector<Fr>&x,const std::vector<Fr>*master){
 need(io_.prover()?(bool(base) && prepared && rng_):(!base && !prepared && !rng_),"category P/V private boundary");
 need(layout_.category_mode && !x.empty() && x.size()==layout_.categories[0].actual,"category X dimensions");
 if(master){
  need(master->size()==layout_.master_actual,"category native sealed source length");
  base=[master](std::size_t index){return index<master->size()?(*master)[index]:Fr(0);};
 }
 single_x_=x;auto plan=singlePlan(layout_,x);roots_.resize(layout_.roots.size());std::set<std::string>used;
 for(std::size_t k=0;k<roots_.size();++k){auto&root=roots_[k];root.spec=layout_.roots[k];need(root.spec.actual && root.spec.padded==root.spec.actual,"category roots cannot carry root padding");need(root.spec.actual-1<=std::numeric_limits<std::uint32_t>::max(),"category root uint32 index overflow");if(!io_.prover())continue;
  auto spans=std::make_shared<std::vector<Borrow>>();
  if(root.spec.registered){
   for(const auto&r:layout_.base_runs)if(r.start.root==k)spans->push_back({r.start.index,r.count,[base,at=r.begin](std::size_t j){return base(at+j);},
    master,master?master->data():nullptr,master?master->size():0,r.begin});
  }else for(const auto&[id,v]:layout_.views)if(!v.mapped)for(const auto&r:v.pieces)if(r.start.root==k){
   auto it=prepared->find(id);need(it!=prepared->end() && it->second && it->second->size()==v.padded && r.begin<=v.padded && r.count<=v.padded-r.begin,"category helper storage identity");
   auto values=it->second;spans->push_back({r.start.index,r.count,[values,at=r.begin](std::size_t j){return values->at(at+j);},
    values.get(),values->data(),values->size(),r.begin});used.insert(id);
  }
  std::sort(spans->begin(),spans->end(),[](const Borrow&a,const Borrow&b){return a.begin<b.begin;});std::size_t covered=0;
  for(const auto&s:*spans){need(s.begin==covered && s.count && s.count<=root.spec.actual-covered,"category borrowed coverage/alias");covered+=s.count;}need(covered==root.spec.actual,"category borrowed source omission");
  root.value=[spans,n=root.spec.actual](std::size_t j){need(j<n,"category borrowed source bound");auto it=std::upper_bound(spans->begin(),spans->end(),j,[](std::size_t at,const Borrow&s){return at<s.begin;});need(it!=spans->begin(),"category borrowed prefix");--it;need(j-it->begin<it->count,"category borrowed gap");return it->value(j-it->begin);};
  root.sealed_span=[spans,n=root.spec.actual](std::size_t begin,std::size_t count){
   need(count && begin<n && count<=n-begin,"category sealed span bound");
   auto it=std::upper_bound(spans->begin(),spans->end(),begin,[](std::size_t at,const Borrow&s){return at<s.begin;});
   need(it!=spans->begin(),"category sealed span prefix");--it;
   return it->bounded(begin-it->begin,count);
  };
  root.sealed_stored=root.spec.actual;root.sealed_storage_valid=[spans]{return !spans->empty();};
 }
 if(io_.prover()){
  need(used.size()==prepared->size(),"category unused helper object");prepared->clear();
  std::vector<Value>sources;std::vector<single::SourceWindow>windows;for(const auto&s:plan.segments){
   unsigned category=0;const char*names[]{"X","W","Aux","LookupQueries","LookupHelpers"};while(category<5 && s.source_id!=names[category])++category;need(category<5,"category source identity");
   std::vector<const Root*>pieces;
   for(const auto&r:roots_)if(r.spec.category==category)pieces.push_back(&r);
   std::sort(pieces.begin(),pieces.end(),[](const auto*a,const auto*b){return a->spec.category_offset<b->spec.category_offset;});
   std::size_t covered=0;
   for(const auto*r:pieces){need(r->spec.category_offset==covered,"category storage gap/alias");covered+=r->spec.actual;}
   need(covered==s.actual,"category storage complete actual");
   // Capture the sealed callbacks by value. Single Hyrax retains the same
   // immutable source backing through both the commit and the final opening.
   struct Piece {std::size_t begin,count;Value read;single::SourceWindow window;};
   auto spans=std::make_shared<std::vector<Piece>>();
   for(const auto*r:pieces)spans->push_back({r->spec.category_offset,r->spec.actual,r->value,r->sealed_span});
   auto locate=[spans](std::size_t j)->const Piece&{
    auto it=std::upper_bound(spans->begin(),spans->end(),j,[](std::size_t at,const Piece&r){return at<r.begin;});
    need(it!=spans->begin(),"category storage prefix");return *--it;
   };
   sources.push_back([locate,n=s.actual,padded=s.padded](std::size_t j){
    need(j<padded,"category logical source bound");if(j>=n)return Fr(0);
    const auto&piece=locate(j);return piece.read(j-piece.begin);
   });
   windows.push_back([locate,n=s.actual](std::size_t j,std::size_t count){
    need(count && j<n && count<=n-j,"category logical source window bound");
    const auto&piece=locate(j);const auto at=j-piece.begin;
    return piece.window(at,std::min(count,piece.count-at));
   });
  }
  unsigned workers=defaultCommitWorkers();if(auto text=std::getenv("ZKGPT_SINGLE_COMMIT_THREADS")){std::string n(text);need(!n.empty() && n.find_first_not_of("0123456789")==std::string::npos,"category worker syntax");auto value=std::stoul(n);need(value && value<=32,"category worker count");workers=unsigned(value);}
  single_secret_=single::commit(plan,std::move(sources),*rng_,workers,std::move(windows));
 }
 single_pc_=single::exchange(io_,plan,io_.prover()?&*single_secret_:nullptr);sealed_=true;
}
}
