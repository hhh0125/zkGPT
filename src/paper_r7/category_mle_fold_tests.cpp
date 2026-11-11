#include "category_tiles.hpp"
#include "single_hyrax.hpp"
#include "public_k.hpp"
#include "u_fold_state.hpp"
#include <random>
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;
namespace ui=oneblock::unified;namespace cl=oneblock::classified;
// Independent, readable global-coordinate reference. No production weights,
// selectors, mapped terms, folding or MLE helper is called here.
Fr denseMLE(const std::vector<Fr>&values,const std::vector<Fr>&point){
 need(values.size()==(std::size_t(1)<<point.size()),"dense dimension");Fr out=0;
 for(std::size_t z=0;z<values.size();++z){Fr chi=1;for(std::size_t j=0;j<point.size();++j)chi*=((z>>j)&1)?point[j]:Fr(1)-point[j];out+=values[z]*chi;}return out;
}
void fold(std::vector<Fr>&v,const Fr&r){for(std::size_t j=0;j<v.size()/2;++j)v[j]=(Fr(1)-r)*v[2*j]+r*v[2*j+1];v.resize(v.size()/2);}
int main(){try{r1::init();std::mt19937 random(20261108);std::size_t rounds=0;
 for(unsigned trial=0;trial<220;++trial){
  std::array<std::size_t,5>a{};for(auto&n:a)n=random()%32;if(trial<5)a={1,2,3,4,5};if(trial==5)a={0,1,0,0,0};
  if(a==std::array<std::size_t,5>{})a[0]=1;
  auto plan=single::makeCategoryPlan(a,std::string(64,'a'));cl::Layout layout;layout.category_mode=true;
  std::vector<Fr>u(plan.domain,Fr(0)),k(plan.domain,Fr(0)),physical(plan.physical_domain,Fr(0));
  const char*names[]{"X","W","Aux","LookupQueries","LookupHelpers"};
  for(auto s:plan.segments){unsigned c=0;while(s.source_id!=names[c])++c;layout.categories[c]={s.actual,s.padded,s.offset,s.physical_offset};
   const auto cap=std::size_t(1)<<(trial%5);for(std::size_t j=0;j<s.actual;){auto count=std::min(cap,s.actual-j);layout.roots.push_back({names[c],count,count,c!=4,c==1,c,j});j+=count;}
   for(std::size_t j=0;j<s.actual;++j){u[s.offset+j]=Fr(int(random()%101)-50);k[s.offset+j]=Fr(int(random()%37)-18);physical[s.physical_offset+j]=u[s.offset+j];}
  }
  auto tiles=cl::categoryTiles(layout);auto independent=cl::categoryTiles(layout);need(tiles.size()==independent.size(),"independent tile rebuild");
  std::vector<ui::Segment>segments;std::vector<unsigned>coverage(plan.domain,0);std::vector<std::vector<Fr>>tu,tk;
  for(std::size_t z=0;z<tiles.size();++z){auto t=tiles[z];need(t.segment.offset==independent[z].segment.offset && t.segment.padded==independent[z].segment.padded,"verifier tile metadata");segments.push_back(t.segment);tu.emplace_back();tk.emplace_back();
   for(std::size_t j=0;j<t.segment.padded;++j){auto global=t.segment.offset+j;need(++coverage[global]==1,"overlapping tile");auto&r=layout.roots[t.root];need(t.begin+j<r.actual && global==layout.categories[r.category].logical_offset+r.category_offset+t.begin+j,"tile global/local mapping");tu.back().push_back(u[global]);tk.back().push_back(k[global]);}
  }
  for(auto s:plan.segments)for(std::size_t j=0;j<s.padded;++j)need(coverage[s.offset+j]==(j<s.actual?1u:0u),"actual coverage or category zero tail");
  std::vector<Fr>q(single::bits(plan.domain));for(auto&r:q)r=trial%4?Fr(int(random()%19)-9):Fr(random()%2);
  std::vector<Fr>coeff(plan.physical_domain,Fr(0));single::mappedQueryTerms(plan,q,[&](const single::QueryTerm&t){for(std::size_t x=0;x<t.left.size();++x)for(std::size_t y=0;y<t.right.size();++y)coeff[(t.first_row+x)*plan.columns+y]+=t.left[x]*t.right[y];});
  Fr mapped=0;for(std::size_t j=0;j<physical.size();++j)mapped+=physical[j]*coeff[j];need(mapped==denseMLE(u,q),"physical mapped MLE != independent dense global MLE");
  for(std::size_t j=0;j<coeff.size();++j){Fr chi=1;for(std::size_t z=0;z<q.size();++z)chi*=((j>>z)&1)?q[z]:Fr(1)-q[z];need(coeff[j]==chi,"direct logical coefficient including zero positions");}
  auto original=tu,local=tu,localK=tk;auto du=u,dk=k;std::vector<Fr>prefix;
  std::uint64_t read_count=0;ui::Inputs inputs=[&](std::size_t z){ui::InputSpan s;s.stored=s.padded=original[z].size();s.storage_identity=&original[z];s.read=[&,z](std::size_t j){++read_count;return original[z].at(j);};return s;};
  ui::PublicK state(segments,[&](std::size_t z){return tk.at(z);},{trial%2?128u:0u,0},"dense",plan.domain);
  ui::FoldedU cache(plan.domain,segments,inputs,"dense");
  while(du.size()>1){
   std::array<Fr,3>eval{Fr(0),Fr(0),Fr(0)};for(unsigned x=0;x<3;++x)for(std::size_t j=0;j<du.size()/2;++j)eval[x]+=((Fr(1)-Fr(x))*du[2*j]+Fr(x)*du[2*j+1])*((Fr(1)-Fr(x))*dk[2*j]+Fr(x)*dk[2*j+1]);
   Fr c2=(eval[2]-Fr(2)*eval[1]+eval[0])/Fr(2);std::array<Fr,3>want{eval[0],eval[1]-eval[0]-c2,c2};
   ui::Factors factors=[&](std::size_t z,const ui::Consume&consume){state.withRoot(z,"dense",[&](const Value&b){for(std::size_t j=0;j<localK[z].size();++j)need(b(j)==localK[z][j],"production K each round != dense local K");consume([&,z](std::size_t j){return original[z][j];},b);});};
   ui::UOptions options;options.threads=1;options.tile_pairs=3;options.observe=[&](std::size_t z,std::size_t first,const Fr*v,std::size_t count){for(std::size_t j=0;j<count;++j)need(v[j]==local[z][first+j],"streamed U each round != independent fold");};
   auto before_reads=read_count;auto got=ui::quadraticRoundStreaming(plan.domain,segments,prefix,factors,inputs,options);
   need(read_count-before_reads==plan.physical_values,"callback source read amplification");
   auto before_cached=read_count;auto was_cached=cache.cached();auto with_cache=ui::quadraticRoundStreaming(plan.domain,segments,prefix,factors,inputs,options,nullptr,cache.enabled()?&cache:nullptr,"dense");
   need(with_cache.coefficients==got.coefficients && with_cache.input_pair==got.input_pair && with_cache.kernel_pair==got.kernel_pair,"cached/streaming global round mismatch");
   need(read_count-before_cached==(was_cached?0:plan.physical_values),"cached read amplification");
   need(got.coefficients==want,"dyadic global polynomial with collapsed tile collisions");
   if(du.size()==2)need(got.input_pair==std::array<Fr,2>{du[0],du[1]} && got.kernel_pair==std::array<Fr,2>{dk[0],dk[1]},"global terminal pair");
   Fr challenge=q[prefix.size()];state.advance("dense",challenge);if(cache.enabled())cache.advance("dense",challenge,inputs);prefix.push_back(challenge);fold(du,challenge);fold(dk,challenge);
   for(auto*group:{&local,&localK})for(auto&v:*group)if(v.size()>1)fold(v,challenge);++rounds;
  }
  need(state.endpoint("dense",prefix)==dk[0] && dk[0]==denseMLE(k,q) && du[0]==denseMLE(u,q),"K/U endpoint != independent dense MLE");
 }
 std::cout<<"CATEGORY_MLE_FOLD_PASS cases=220 round_comparisons="<<rounds<<"\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
