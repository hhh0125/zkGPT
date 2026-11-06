#include "static_source_map.hpp"
#include <algorithm>
namespace zkgpt::r7::oneblock::classified {
StaticBaseMap::StaticBaseMap(const Layout&p):size_(p.master_actual),runs_(p.base_runs){
 need(size_ && size_<=(1u<<28) && p.base.empty(),"PERF4 static compressed base shape");std::size_t covered=0;
 for(const auto&r:runs_){need(r.begin==covered && r.count && r.count<=size_-covered && r.start.root<p.roots.size(),"PERF4 static base coverage");const auto actual=p.roots[r.start.root].actual;need(actual<=(1ull<<32) && r.start.index<=actual && r.count<=actual-r.start.index,"PERF4 static base physical bounds");covered+=r.count;}
 need(covered==size_,"PERF4 static base omission");directory_.resize((size_+4095)/4096);std::size_t k=0;
 for(std::size_t bucket=0;bucket<directory_.size();++bucket){auto at=bucket*4096;while(at>=runs_[k].begin+runs_[k].count)++k;need(k<=UINT32_MAX,"PERF4 static directory index");directory_[bucket]=k;}
}
Coordinate StaticBaseMap::at(std::size_t at)const{
 need(at<size_,"partitioned base bounds");auto k=directory_[at/4096];while(at>=runs_[k].begin+runs_[k].count)++k;const auto&r=runs_[k];return {r.start.root,std::uint32_t(r.start.index+at-r.begin)};
}
StaticSourceMap::StaticSourceMap(const Layout&p,const View&v,const StaticBaseMap&base):padded_(v.padded){
 need(padded_ && v.actual<=padded_,"PERF4 static view shape");std::size_t covered=0;
 auto append=[&](std::size_t count,Located first){
  need(count && count<=padded_-covered && first.bias.isValid(),"PERF4 static view coverage/bias");
  if(!first.constant){need(first.coordinate.root<p.roots.size(),"PERF4 static view root");auto actual=p.roots[first.coordinate.root].actual;need(actual<=(1ull<<32) && first.coordinate.index<=actual && count<=actual-first.coordinate.index,"PERF4 static view physical bounds");}
  else first.coordinate={};
  if(!runs_.empty()){auto&last=runs_.back();if(last.first.constant==first.constant && last.first.bias==first.bias && (first.constant || (last.first.coordinate.root==first.coordinate.root && std::uint64_t(last.first.coordinate.index)+last.count==first.coordinate.index))){last.count+=count;covered+=count;return;}}
  runs_.push_back({covered,count,first});covered+=count;
 };
 if(v.mapped){std::size_t consumed=0;for(const auto&span:v.spans){if(consumed==v.actual)break;auto count=std::min(span.count,v.actual-consumed);if(!count)continue;need(span.offset<p.master_actual && count<=p.master_actual-span.offset,"classified trusted map bounds");auto at=span.offset,end=at+count;
   while(at<end){const auto&runs=base.runs();auto it=std::upper_bound(runs.begin(),runs.end(),at,[](std::size_t j,const Piece&r){return j<r.begin;});need(it!=runs.begin(),"PERF4 static source prefix");--it;auto n=std::min(end-at,it->count-(at-it->begin));append(n,{base.at(at),span.bias,false});at+=n;}consumed+=count;
  }need(consumed==v.actual,"classified missing source span");if(covered<padded_)append(padded_-covered,{{},Fr(0),true});
 }else{
  // Partitioned identity binds Pieces, not the unused legacy View.start.
  // The caller deliberately keeps piece-less helper callbacks on reference.
  need(!v.pieces.empty(),"PERF4 static helper requires bound pieces");auto pieces=v.pieces;std::sort(pieces.begin(),pieces.end(),[](const Piece&a,const Piece&b){return a.begin<b.begin;});for(const auto&piece:pieces){need(piece.begin==covered,"PERF4 helper gap/overlap");append(piece.count,{piece.start,Fr(0),false});}
 }
 need(covered==padded_,"PERF4 static view omission");
}
Located StaticSourceMap::at(std::size_t j)const{
 need(j<padded_,"classified view query bounds");const Run*r=&runs_[0];if(runs_.size()!=1){auto it=std::upper_bound(runs_.begin(),runs_.end(),j,[](std::size_t at,const Run&run){return at<run.begin;});--it;r=&*it;}auto out=r->first;if(!out.constant)out.coordinate.index+=j-r->begin;return out;
}
}
