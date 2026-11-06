#include "classified_commitments.hpp"
#include <algorithm>
#include <cstdlib>
#include <set>
namespace zkgpt::r7::oneblock::classified {
single::Plan singlePlan(const Layout&layout,const std::vector<Fr>&x){
 need(!x.empty() && x.size()<=32768,"single frozen public X maximum 32768");std::vector<unified::Block>blocks;std::vector<std::size_t>actual{x.size()};
 for(auto&r:layout.roots){unsigned category=r.model?1:(r.id.rfind("C_aux",0)==0?2:3);need(r.model || r.id.rfind("C_aux",0)==0 || r.id.rfind("C_lookup/",0)==0,"single unknown native source category");blocks.push_back({category,r.padded});actual.push_back(r.actual);}
 std::vector<std::string>ids{"public/X"};for(const auto&r:layout.roots)ids.push_back(r.id);
 std::vector<single::PublicCell>cells;need(baseSize(layout)>=x.size(),"single native X source bounds");for(std::size_t j=0;j<x.size();++j){auto c=baseCoordinate(layout,j);cells.push_back({std::size_t(c.root)+1,c.index,j});}
 return single::makePlan(unified::pack(blocks,power2(x.size())),actual,identity(layout),ids,cells);
}
void Commitments::initializeSingle(Value base,Prepared*prepared,const std::vector<Fr>&x){
 need(io_.prover()?(bool(base) && prepared && rng_):(!base && !prepared && !rng_),"single classified P/V boundary");
 single_x_=x;need(baseSize(layout_)>=x.size(),"single native public X prefix bounds");
 single_public_roots_.resize(layout_.roots.size(),0);
 for(std::size_t j=0;j<x.size();++j){auto c=baseCoordinate(layout_,j);auto key=(std::uint64_t(c.root)<<32)|c.index;need(single_public_coordinates_.emplace(key,j).second,"single native X coordinate alias");single_public_roots_.at(c.root)=1;}
 auto plan=singlePlan(layout_,x);roots_.resize(layout_.roots.size());std::set<std::string>used;
 for(std::size_t k=0;k<roots_.size();++k){auto&root=roots_[k];root.spec=layout_.roots[k];if(!io_.prover())continue;
  // Source partitions retain exact original bytes/coordinates, but have NO
  // individual PC, blinding vector, generator basis or opening callback.
  // Only Source actual cells are stored. This includes any nonzero helper
  // view padding inside the source; the Source suffix remains implicit zero.
  auto values=std::make_shared<std::vector<Fr>>(root.spec.actual,Fr(0));
  if(root.spec.registered){
   if(layout_.master_actual){for(auto&run:layout_.base_runs)if(run.start.root==k){need(run.start.index<=root.spec.actual && run.count<=root.spec.actual-run.start.index,"single native source run bounds");for(std::size_t j=0;j<run.count;++j)values->at(run.start.index+j)=base(run.begin+j);}}
   else for(std::size_t j=0;j<layout_.base.size();++j)if(layout_.base[j].root==k){need(layout_.base[j].index<root.spec.actual,"single dense native source bounds");values->at(layout_.base[j].index)=base(j);}
  }else for(auto&[id,view]:layout_.views)if(!view.mapped){
   auto copy=[&](std::size_t from,std::size_t count,std::size_t into){auto it=prepared->find(id);need(it!=prepared->end() && it->second && it->second->size()==view.padded && from<=view.padded && count<=view.padded-from && into<=root.spec.actual && count<=root.spec.actual-into,"single prepared source bounds");std::copy_n(it->second->begin()+from,count,values->begin()+into);used.insert(id);};
   if(!view.pieces.empty()){for(auto&piece:view.pieces)if(piece.start.root==k)copy(piece.begin,piece.count,piece.start.index);}
   else if(view.start.root==k)copy(0,view.padded,view.start.index);
  }
  for(const auto&c:plan.public_cells)if(c.source==k+1)values->at(c.index)=x.at(c.x_index);
  const auto actual=root.spec.actual,padded=root.spec.padded;root.value=[this,k,values,actual,padded](std::size_t j){need(j<padded,"single logical source bound");if(j>=actual)return Fr(0);auto l=publicLocation({{std::uint32_t(k),std::uint32_t(j)},Fr(0),false});return l.constant?l.bias:values->at(j);};root.sealed_data=values->data();root.sealed_stored=actual;root.sealed_storage_valid=[values,data=values->data(),size=values->size()]{return values->data()==data && values->size()==size;};
 }
 if(io_.prover()){
  need(used.size()==prepared->size(),"single unused/missing prepared source");prepared->clear();
  std::vector<Value>sources;sources.push_back([this](std::size_t j){return j<single_x_.size()?single_x_[j]:Fr(0);});for(auto&r:roots_)sources.push_back(r.value);
  unsigned workers=1;if(auto text=std::getenv("ZKGPT_SINGLE_COMMIT_THREADS")){auto n=std::string(text);need(!n.empty() && std::all_of(n.begin(),n.end(),[](char c){return c>='0' && c<='9';}),"single worker integer");auto parsed=std::stoul(n);need(parsed>0 && parsed<=32,"single worker bound 1..32");workers=unsigned(parsed);}
  single_secret_=single::commit(plan,std::move(sources),*rng_,workers);
 }
 single_pc_=single::exchange(io_,plan,io_.prover()?&*single_secret_:nullptr);sealed_=true;
}
Located Commitments::publicLocation(Located l)const{
 if(static_source_enabled_ && !l.constant && l.coordinate.root<single_public_roots_.size() && !single_public_roots_[l.coordinate.root])return l;
 if(!l.constant && !single_public_coordinates_.empty()){auto key=(std::uint64_t(l.coordinate.root)<<32)|l.coordinate.index;auto found=single_public_coordinates_.find(key);if(found!=single_public_coordinates_.end()){l.bias+=single_x_.at(found->second);l.constant=true;l.coordinate={};}}
 return l;
}
}
