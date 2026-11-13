from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');P=R/'src/paper_r7';S=R/'reports/R7-MLE-FOLD-20261108-v1'
assert 'candidate expects 5 arguments, 6 provided' in (S/'cache-RED-api.log').read_text()
p=P/'u_fold_state.hpp';s=p.read_text().replace('std::uint64_t budget);','std::uint64_t budget,bool deferred_callbacks=false);')
s=s.replace('bool enabled()const;','bool enabled()const;bool cached()const;\n void capture(std::size_t,std::size_t,const Fr*,std::size_t);')
p.write_text(s)
p=P/'u_fold_state.cpp';s=p.read_text()
s=s.replace('a.padded==b.padded;', 'a.padded==b.padded && bool(a.read)==bool(b.read) && a.storage_identity==b.storage_identity && a.storage_offset==b.storage_offset;')
s=s.replace('void shape(InputSpan s,std::size_t padded){need(s.padded==padded && s.stored<=s.padded && (!s.stored || s.data),','void shape(InputSpan s,std::size_t padded,bool deferred){need(s.padded==padded && s.stored<=s.padded && (!s.stored || s.data || (deferred && s.read && s.storage_identity)),')
s=s.replace('bool seen=false;','bool seen=false;std::size_t captured=0;')
s=s.replace('std::size_t n;unsigned bits=0;', 'bool deferred=false;unsigned admission=0;std::size_t n;unsigned bits=0;')
s=s.replace('std::uint64_t budget):n(domain),cap(budget),session(id)', 'std::uint64_t budget,bool callbacks):deferred(callbacks),n(domain),cap(callbacks?std::min<std::uint64_t>(budget,512ull<<20):budget),session(id)')
old='''  if(!bits || first>cap || meta>cap-first)return; // Public admission, before any retained private vector.'''
new='''  if(deferred){
   // Public checkpoint selection only. Never retain the complete original U
   // or its first half: wait until actual tile contractions fit this budget.
   for(admission=0;admission<bits;++admission){first=0;for(auto segment:s)first+=std::max<std::size_t>(1,segment.padded>>std::min(admission,r1::log2exact(segment.padded)))*sizeof(Fr);if(first<=cap && meta<=cap-first)break;}
   if(admission==bits)return;
  }
  if(!bits || first>cap || meta>cap-first)return; // Public admission, before any retained private vector.'''
assert old in s;s=s.replace(old,new).replace('shape(original,s[k].padded);','shape(original,s[k].padded,deferred);')
s=s.replace('std::uint64_t cap):p_(std::make_unique<Impl>(n,s,i,id,cap)){}','std::uint64_t cap,bool deferred):p_(std::make_unique<Impl>(n,s,i,id,cap,deferred)){}')
s=s.replace('bool FoldedU::enabled()const{return p_->enabled;}', 'bool FoldedU::enabled()const{return p_->enabled;}\nbool FoldedU::cached()const{return p_->enabled && !p_->failed && p_->point.size()>(p_->deferred?p_->admission:0);}')
s=s.replace('if(prefix.empty()){return r.original;}', '''if(p_->deferred && prefix.size()==p_->admission){
  auto count=std::max<std::size_t>(1,r.original.padded>>std::min<unsigned>(prefix.size(),r1::log2exact(r.original.padded)));
  need(r.values.empty() && count*sizeof(Fr)<=p_->cap-p_->used,"category U checkpoint preflight");r.values.resize(count);
  auto bytes=r.values.capacity()*sizeof(Fr);need(bytes<=p_->cap-p_->used,"category U checkpoint capacity");p_->used+=bytes;p_->m.capacity_peak_bytes=std::max(p_->m.capacity_peak_bytes,p_->used);r.captured=0;++p_->m.roots_materialized;
 }
 if(!cached()){return r.original;}''')
# Legacy caches fold after round zero. Their first span must remain original.
# The same cached() predicate is exactly point.size()>0 for that mode.
anchor='void FoldedU::completeRound('
pos=s.index(anchor)
s=s[:pos]+'''void FoldedU::capture(std::size_t k,std::size_t first,const Fr*values,std::size_t count){try{
 if(!p_->deferred || p_->point.size()!=p_->admission)return;
 need(p_->enabled && !p_->failed && p_->consuming && k<p_->roots.size() && values,"category U capture phase");auto&r=p_->roots[k];
 need(first==r.captured && count<=r.values.size()-r.captured,"category U checkpoint coverage");std::copy_n(values,count,r.values.begin()+first);r.captured+=count;p_->m.fold_write_coordinates+=count;
}catch(...){poison();throw;}}
'''+s[pos:]
s=s.replace('for(const auto&r:p_->roots)need(r.seen,"PERF4 U incomplete root consumption");','for(const auto&r:p_->roots){need(r.seen,"PERF4 U incomplete root consumption");if(p_->deferred && prefix.size()==p_->admission)need(r.captured==r.values.size(),"category U incomplete checkpoint");}')
s=s.replace('for(auto&root:p_->roots){\n   if(p_->point.empty()){', '''for(auto&root:p_->roots){
   if(p_->deferred){
    if(p_->point.size()>=p_->admission && root.values.size()>1){auto n=root.values.size();for(std::size_t j=0;j<n/2;++j)root.values[j]=root.values[2*j]+challenge*(root.values[2*j+1]-root.values[2*j]);root.values.resize(n/2);p_->m.cache_fold_read_coordinates+=n;p_->m.fold_input_coordinates+=n;p_->m.fold_write_coordinates+=n/2;p_->m.multiplications+=n/2;p_->m.additions+=n;}
   }else if(p_->point.empty()){''')
s=s.replace('<<",\\"private_files\\":0}', '<<",\\"deferred_callback_mode\\":"<<(p_->deferred?"true":"false")<<",\\"checkpoint_round\\":"<<p_->admission<<",\\"private_files\\":0}')
p.write_text(s)
p=P/'u_streaming.cpp';s=p.read_text().replace('const bool cached=folded && !prefix.empty();','const bool cached=folded && folded->cached();')
s=s.replace('std::size_t planned=segments.size()*192+16384+', 'std::size_t planned=options.threads*1024*sizeof(Fr)+segments.size()*192+16384+')
s=s.replace('const auto length=s.padded>>low;', 'if(inputs){if(cached)++m.state_hits;else ++m.state_misses;}\n  const auto length=s.padded>>low;')
s=s.replace('if(options.observe)options.observe(k,first,tile.data(),count);', 'if(options.observe)options.observe(k,first,tile.data(),count);\n    if(folded)folded->capture(k,first,tile.data(),count);')
p.write_text(s)
p=P/'unified_input.cpp';s=p.read_text();old='std::unique_ptr<FoldedU>folded_u;if(io.prover() && state && inputs && contiguous_inputs && FoldedU::requested())folded_u=std::make_unique<FoldedU>(n,segments,inputs,io.session(),FoldedU::environmentBudget());'
new='''bool category_cache=true;if(const auto*v=std::getenv("ZKGPT_CATEGORY_U_FOLD")){std::string text(v);need(text=="0" || text=="1","category U cache switch");category_cache=text=="1";}
 std::uint64_t category_budget=512ull<<20;if(const auto*v=std::getenv("ZKGPT_CATEGORY_U_RAM_BYTES")){std::string text(v);need(!text.empty() && text.find_first_not_of("0123456789")==std::string::npos,"category U budget syntax");category_budget=std::stoull(text);need(category_budget<=512ull<<20,"category U budget cap");}
 std::unique_ptr<FoldedU>folded_u;
 if(io.prover() && state && inputs && ((contiguous_inputs && FoldedU::requested()) || (!contiguous_inputs && category_cache)))folded_u=std::make_unique<FoldedU>(n,segments,inputs,io.session(),contiguous_inputs?FoldedU::environmentBudget():category_budget,!contiguous_inputs);'''
assert old in s;s=s.replace(old,new);p.write_text(s)
print('bounded deferred callback cache implemented')
