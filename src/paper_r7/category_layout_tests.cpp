#include "classified_commitments.hpp"
#include "fs_io.hpp"
#include <iostream>
#include <random>
using namespace zkgpt::r7;using namespace oneblock;
int main(){try{
 r1::init();
 auto p=single::makeCategoryPlan({3,5,7,9,11},std::string(64,'b'));
 need(p.segments.size()==5 && p.domain==64 && p.physical_values==35 && p.logical_values==35 && p.physical_domain==p.domain && p.rows*p.columns==p.domain,"five direct logical categories");
 const std::size_t pads[]{4,8,8,16,16},offsets[]{0,4,12,20,36};
 for(std::size_t k=0;k<5;++k)need(p.segments[k].padded==pads[k] && p.segments[k].offset==offsets[k] && p.segments[k].physical_offset==offsets[k] && p.segments[k].physical_length==p.segments[k].actual,"category tail only, committed X");
 auto empty=single::makeCategoryPlan({0,5,0,0,0},std::string(64,'b'));
 need(empty.segments.size()==1 && empty.domain==8 && empty.physical_values==5,"empty categories allocate nothing");
 
 // Independent bit-by-bit coefficients cover all five categories, unaligned
 // boundaries, category-local zero tails and the physical matrix's final row.
 std::mt19937 random(20261106);
 for(unsigned trial=0;trial<120;++trial){
  std::array<std::size_t,5>a{};for(auto&n:a)n=1+random()%43;
  if(trial==0)a={3,5,7,9,11};auto plan=single::makeCategoryPlan(a,std::string(64,'b'));
  std::vector<Fr>q(single::bits(plan.domain));for(auto&r:q)r=Fr(trial%3?2+random()%19:random()%2);
  auto chi=[&](std::size_t j){Fr out=1;for(std::size_t b=0;b<q.size();++b)out*=((j>>b)&1)?q[b]:Fr(1)-q[b];return out;};
  std::vector<Fr>coeff(plan.physical_domain,Fr(0)),expected(plan.physical_domain,Fr(0));
  for(std::size_t j=0;j<plan.domain;++j)expected[j]=chi(j);
  std::size_t terms=0;single::mappedQueryTerms(plan,q,[&](const single::QueryTerm&t){++terms;need(t.first_row==0 && t.left.size()==plan.rows && t.right.size()==plan.columns,"full direct query dimensions");for(std::size_t i=0;i<t.left.size();++i)for(std::size_t j=0;j<t.right.size();++j)coeff[(t.first_row+i)*plan.columns+j]+=t.left[i]*t.right[j];});
  need(terms==1 && coeff==expected,"five-category physical/logical MLE coefficient mismatch");
  if(trial<12){
   auto values=std::make_shared<std::vector<Fr>>(plan.domain,Fr(0));std::vector<Value>sources;
   for(auto segment:plan.segments){for(std::size_t j=0;j<segment.actual;++j)values->at(segment.offset+j)=Fr(random()%100);sources.push_back([values,segment](std::size_t j){return values->at(segment.offset+j);});}
   SessionRandom rng;auto secret=single::commit(plan,sources,rng);
   for(std::size_t row=0;row<plan.rows;++row){std::vector<Fr>cells(values->begin()+row*plan.columns,values->begin()+(row+1)*plan.columns);need(secret.pc.rows[row]==hyrax_a3::vectorCommit(cells,secret.blinds[row]),"row commitment differs from logical matrix including zero padding");}
   need(single::evaluate(secret,q)==r1::mle(*values,q),"committed category MLE differs from independently materialized logical vector");
  }
 }
 // Metadata-only sizes cross two storage limits without allocating witnesses.
 const auto cap=std::size_t(1)<<24;
 auto large=single::makeCategoryPlan({3,2*cap+3,0,0,0},std::string(64,'b'));
 need(large.segments[1].padded==4*cap && large.segments[1].offset==4,"cross-chunk category padding");
 R7Context c;c.layout_sha256=std::string(64,'c');c.plan.master=r1::PCSpec::make("large",2*cap+6,4*cap,r1::Layout::RowMajor);
 c.plan.sources.push_back({r1::PCSpec::make("W",2*cap+3,4*cap,r1::Layout::RowMajor),{{3,2*cap+3,Fr(0)}}});
 auto map=classified::categoryLayout(c,{{0,3,Fr(0)}},cap);
 need(map.categories[1].actual==2*cap+3 && map.categories[1].padded==4*cap && map.roots.size()==4,"storage chunk crossing added logical padding");
 for(auto&r:map.roots)need(r.actual==r.padded && r.actual<=cap,"storage-only chunk must have no padded tail");
 bool rejected=false;try{single::makeCategoryPlan({0,0,0,0,0},std::string(64,'b'));}catch(const std::exception&){rejected=true;}need(rejected,"all-empty witness must fail explicitly");
 auto corrupted=large;corrupted.segments[1].offset=8;rejected=false;try{single::descriptor(corrupted);}catch(const std::exception&){rejected=true;}need(rejected,"category alignment gaps cannot be prover-selected");
 corrupted=large;corrupted.public_cells.push_back({1,0,0});rejected=false;try{single::descriptor(corrupted);}catch(const std::exception&){rejected=true;}need(rejected,"category layout accepted old public holes");
 std::cout<<"CATEGORY_LAYOUT_TESTED random_MLE=120 cross_chunk_limit=2^24\n";return 0;
}catch(const std::exception&e){std::cerr<<"CATEGORY_FAILED "<<e.what()<<'\n';return 1;}}
