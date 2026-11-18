#pragma once
#include "paper_r2_model_queries.hpp"
#include <cstdlib>
#include <algorithm>
namespace zkgpt::r2 {
inline bool largeRangeSplitEnabled(){const char*v=std::getenv("ZKGPT_RANGE_SPLIT_LARGE");if(!v)return false;r1::require((v[0]=='0'||v[0]=='1')&&!v[1],"ZKGPT_RANGE_SPLIT_LARGE must be 0 or 1");return v[0]=='1';}
inline std::vector<std::size_t>rangeSplitSizes(std::size_t n,bool enabled){
 r1::require(n>0 && n<=r1::kMaxVector,"range split size bound");
 std::size_t padded=1;while(padded<n)padded*=2;
 if(!enabled || n<(1u<<22) || padded==n)return {n};
 const auto first=padded/2;std::size_t last=2;while(last<n-first)last*=2;
 if((padded-first-last)*4<padded)return {n};
 return {first,n-first};
}
inline std::vector<std::vector<r1::InputSpan>>splitRangeSpans(const std::vector<r1::InputSpan>&spans,const std::vector<std::size_t>&sizes){
 std::size_t total=0,want=0;for(auto&s:spans){r1::require(s.count && s.count<=r1::kMaxVector && total<=r1::kMaxVector-s.count,"range span size");total+=s.count;}
 for(auto n:sizes){r1::require(n && want<=total && n<=total-want,"range split coverage");want+=n;}r1::require(want==total,"range split omission");
 std::vector<std::vector<r1::InputSpan>>out;std::size_t k=0,used=0;
 for(auto n:sizes){out.emplace_back();while(n){r1::require(k<spans.size(),"range split source bounds");const auto&s=spans[k];auto take=std::min(n,s.count-used);out.back().push_back({s.offset+used,take,s.bias});n-=take;used+=take;if(used==s.count){++k;used=0;}}}
 r1::require(k==spans.size() && !used,"range split trailing source");return out;
}
}
