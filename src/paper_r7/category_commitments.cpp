#include "classified_commitments.hpp"
#include <algorithm>
#include <cstdlib>
namespace zkgpt::r7::oneblock::classified {
namespace {
struct Borrow {std::size_t begin,count;Value value;};
}
void Commitments::initializeCategory(Value base,Prepared*prepared,const std::vector<Fr>&x){
 need(io_.prover()?(bool(base) && prepared && rng_):(!base && !prepared && !rng_),"category P/V private boundary");
 need(layout_.category_mode && !x.empty() && x.size()==layout_.categories[0].actual,"category X dimensions");
 single_x_=x;auto plan=singlePlan(layout_,x);roots_.resize(layout_.roots.size());std::set<std::string>used;
 for(std::size_t k=0;k<roots_.size();++k){auto&root=roots_[k];root.spec=layout_.roots[k];need(root.spec.actual && root.spec.padded==root.spec.actual,"category chunks cannot carry root padding");if(!io_.prover())continue;
  auto spans=std::make_shared<std::vector<Borrow>>();
  if(root.spec.registered){
   for(const auto&r:layout_.base_runs)if(r.start.root==k)spans->push_back({r.start.index,r.count,[base,at=r.begin](std::size_t j){return base(at+j);}});
  }else for(const auto&[id,v]:layout_.views)if(!v.mapped)for(const auto&r:v.pieces)if(r.start.root==k){
   auto it=prepared->find(id);need(it!=prepared->end() && it->second && it->second->size()==v.padded && r.begin<=v.padded && r.count<=v.padded-r.begin,"category helper storage identity");
   auto values=it->second;spans->push_back({r.start.index,r.count,[values,at=r.begin](std::size_t j){return values->at(at+j);}});used.insert(id);
  }
  std::sort(spans->begin(),spans->end(),[](const Borrow&a,const Borrow&b){return a.begin<b.begin;});std::size_t covered=0;
  for(const auto&s:*spans){need(s.begin==covered && s.count && s.count<=root.spec.actual-covered,"category borrowed coverage/alias");covered+=s.count;}need(covered==root.spec.actual,"category borrowed source omission");
  root.value=[spans,n=root.spec.actual](std::size_t j){need(j<n,"category borrowed source bound");auto it=std::upper_bound(spans->begin(),spans->end(),j,[](std::size_t at,const Borrow&s){return at<s.begin;});need(it!=spans->begin(),"category borrowed prefix");--it;need(j-it->begin<it->count,"category borrowed gap");return it->value(j-it->begin);};
  root.sealed_stored=root.spec.actual;root.sealed_storage_valid=[spans]{return !spans->empty();};
 }
 if(io_.prover()){
  need(used.size()==prepared->size(),"category unused helper object");prepared->clear();
  std::vector<Value>sources;for(const auto&s:plan.segments){
   unsigned category=0;const char*names[]{"X","W","Aux","LookupQueries","LookupHelpers"};while(category<5 && s.source_id!=names[category])++category;need(category<5,"category source identity");
   auto spans=std::make_shared<std::vector<Borrow>>();for(const auto&r:roots_)if(r.spec.category==category)spans->push_back({r.spec.category_offset,r.spec.actual,r.value});
   std::sort(spans->begin(),spans->end(),[](const Borrow&a,const Borrow&b){return a.begin<b.begin;});std::size_t covered=0;for(const auto&r:*spans){need(r.begin==covered,"category storage gap/alias");covered+=r.count;}need(covered==s.actual,"category storage complete actual");
   sources.push_back([spans,n=s.actual,padded=s.padded](std::size_t j){need(j<padded,"category logical source bound");if(j>=n)return Fr(0);auto it=std::upper_bound(spans->begin(),spans->end(),j,[](std::size_t at,const Borrow&r){return at<r.begin;});need(it!=spans->begin(),"category storage prefix");--it;return it->value(j-it->begin);});
  }
  unsigned workers=1;if(auto text=std::getenv("ZKGPT_SINGLE_COMMIT_THREADS")){std::string n(text);need(!n.empty() && n.find_first_not_of("0123456789")==std::string::npos,"category worker syntax");auto value=std::stoul(n);need(value && value<=32,"category worker count");workers=unsigned(value);}
  single_secret_=single::commit(plan,std::move(sources),*rng_,workers);
 }
 single_pc_=single::exchange(io_,plan,io_.prover()?&*single_secret_:nullptr);sealed_=true;
}
}
