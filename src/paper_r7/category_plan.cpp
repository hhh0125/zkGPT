#include "single_hyrax.hpp"
#include <algorithm>
namespace zkgpt::r7::oneblock::single {
namespace {
constexpr const char*names[]{"X","W","Aux","LookupQueries","LookupHelpers"};
std::size_t pow2(std::size_t n){need(n && n<=maxDomain,"category size overflow");std::size_t p=1;while(p<n)p*=2;return p;}
}
Plan makeCategoryPlan(const std::array<std::size_t,5>&actual,const std::string&id){
 Plan p;p.category_mode=true;p.source_identity=id;std::size_t logical=0,physical=0;
 for(std::size_t k=0;k<5;++k){if(!actual[k])continue;auto padded=pow2(actual[k]);need(logical<=maxDomain-padded && physical<=maxDomain-actual[k],"category cumulative bound");
  p.segments.push_back({logical,actual[k],padded,physical,actual[k],false,names[k]});
  p.runs.push_back({p.segments.size()-1,0,actual[k],physical});logical+=padded;physical+=actual[k];
 }
 need(physical,"category all-empty witness");p.logical_values=physical;p.domain=pow2(logical);
 static_cast<PhysicalCommitmentLayout&>(p)=matrixShape(physical);validateCategoryPlan(p);return p;
}
void validateCategoryPlan(const Plan&p){
 need(p.category_mode && p.public_cells.empty() && p.segments.size() && p.segments.size()<=5 && p.packing.segments.empty(),"category plan mode/holes/count");
 need(p.source_identity.size()==64 && p.source_identity.find_first_not_of("0123456789abcdef")==std::string::npos,"category source identity");
 std::size_t logical=0,physical=0,next=0;need(p.runs.size()==p.segments.size(),"category runs count");
 for(std::size_t i=0;i<p.segments.size();++i){const auto&s=p.segments[i];while(next<5 && s.source_id!=names[next])++next;need(next<5,"category order/duplicate/identity");++next;
  const auto n=pow2(s.actual);need(s.padded==n && s.offset==logical && s.physical_offset==physical && s.physical_length==s.actual && !s.is_public,"category noncanonical extent");
  need(logical<=maxDomain-n && physical<=maxDomain-s.actual,"category extent overflow");logical+=n;physical+=s.actual;
  const auto&r=p.runs[i];need(r.source==i && !r.source_offset && r.count==s.actual && r.physical_offset==s.physical_offset,"category physical mapping");
 }
 auto shape=matrixShape(physical);need(p.domain==pow2(logical) && p.logical_values==physical && p.physical_values==physical && p.physical_domain==shape.physical_domain && p.rows==shape.rows && p.columns==shape.columns,"category global shape");
}
void writeCategoryLayout(std::ostream&o,const Public&p){
 validateCategoryPlan(p.plan);const auto&q=p.plan;
 o<<"{\"suite\":\""<<suite<<"\",\"layout\":\""<<layout<<"\",\"model_scope\":\""<<modelScope<<"\",\"logical_commitments\":1,\"identity\":\""<<identity(p)<<"\",\"source_layout_identity\":\""<<q.source_identity<<"\",\"rows\":"<<q.rows<<",\"columns\":"<<q.columns<<",\"domain\":"<<q.domain<<",\"logical_values\":"<<q.logical_values<<",\"physical_values\":"<<q.physical_values<<",\"physical_domain\":"<<q.physical_domain<<",\"logical_gap\":"<<q.domain-q.logical_values<<",\"physical_padding\":"<<q.physical_domain-q.physical_values<<",\"source_segments\":[";
 for(std::size_t k=0;k<q.segments.size();++k){if(k)o<<',';const auto&s=q.segments[k];o<<"{\"category\":\""<<s.source_id<<"\",\"actual\":"<<s.actual<<",\"padded\":"<<s.padded<<",\"logical_offset\":"<<s.offset<<",\"physical_offset\":"<<s.physical_offset<<",\"physical_length\":"<<s.physical_length<<",\"witness_source\":\"sealed-native-source\"}";}
 o<<"],\"native_public_cells\":[],\"category_alignment_gap\":0,\"root_padding\":0}\n";
}
}
