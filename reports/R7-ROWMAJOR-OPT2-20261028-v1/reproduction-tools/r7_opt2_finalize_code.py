from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==R
P=R/'src/paper_r7'
p=P/'opt2_kernel_tests.cpp';s=p.read_text();old='if(!bench && optimized){auto good=c.kernel(0,lambda,prepared);'
new='''if(!bench && optimized){
   Commitments::Authentication uncached(c.claims_.size());uncached.map_bytes=Commitments::Authentication::map_limit;uncached.weight_bytes=Commitments::Authentication::weight_limit;
   for(unsigned i=0;i<c.claims_.size();++i){c.prepareClaim(c.claims_[i],uncached,i);need(!uncached.claims[i].frozen && !uncached.claims[i].weights,"budget fallback retained cache");}
   for(unsigned k=0;k<6;++k)need(c.kernel(k,lambda,prepared)==c.kernel(k,lambda,uncached),"budget fallback kernel mismatch");need(c.partialDot(lambda,uncached)==dots,"budget fallback dot mismatch");
   Commitments::Authentication tight(c.claims_.size());tight.map_bytes=Commitments::Authentication::map_limit-2*sizeof(Commitments::Authentication::Run);c.prepareClaim(c.claims_[24],tight,24);need(!tight.claims[24].frozen && tight.claims[24].runs.empty(),"fragmented map exceeded cache bound");
   auto good=c.kernel(0,lambda,prepared);'''
assert old in s;s=s.replace(old,new)
old='prepared.claims[0].frozen=false;bool rejected=false;try{c.kernel(0,lambda,prepared);}catch(...){rejected=true;}'
new='prepared.claims[0].frozen=false;c.claims_[0].map=[](std::size_t j)->Located{return {{0,std::uint32_t(j^1)},Fr(0),false};};bool rejected=false;try{c.kernel(0,lambda,prepared);}catch(const std::exception&e){rejected=std::string(e.what())=="classified source map changed after defer";}'
assert old in s;s=s.replace(old,new);p.write_text(s)
p=P/'opt2_sumcheck_tests.cpp';s=p.read_text().replace('Expression spec(unsigned d){if(d==2)', 'Expression spec(unsigned d){if(d==0)return {1,{{Fr(7),{}}}};if(d==1)return {2,{{Fr(3),{0}},{Fr(-5),{1}},{Fr(7),{}}}};if(d==2)');s=s.replace('for(unsigned d:{2u,3u})','for(unsigned d:(bench?std::vector<unsigned>{2,3}:std::vector<unsigned>{0,1,2,3}))');p.write_text(s)
# Print exact counterfactual legacy scan counts from public shapes of this run,
# so the full execution needs no second baseline proof.
p=P/'classified_commitments.cpp';s=p.read_text();needle=' auto initial=combine(session_,"Classified/aggregate",values,lambda);'
new=r''' if(opt2MetricsEnabled()){
  std::uint64_t legacyCoordinates=0,legacyWeights=0,frozen=0,onlyRoot=0,nonRoot=0;
  for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];auto scans=c.only_root>=0?1:roots_.size();legacyCoordinates+=scans*c.padded;legacyWeights+=scans;if(c.only_root>=0)++onlyRoot;else ++nonRoot;if(auth.claims[i].frozen)++frozen;}
  std::cerr<<"OPT2_SCAN_PLAN {\"claims\":"<<claims_.size()<<",\"root_only_claims\":"<<onlyRoot<<",\"multi_root_claims\":"<<nonRoot<<",\"frozen_claims\":"<<frozen<<",\"legacy_kernel_calls\":"<<roots_.size()*(io_.prover()?2:1)<<",\"legacy_kernel_coordinates\":"<<legacyCoordinates*(io_.prover()?2:1)<<",\"legacy_weights_precomputations\":"<<legacyWeights*(io_.prover()?2:1)+nonRoot<<",\"map_cache_bytes\":"<<auth.map_bytes<<",\"weight_cache_bytes\":"<<auth.weight_bytes<<"}\n";
 }
'''+needle
assert needle in s;s=s.replace(needle,new);p.write_text(s)
print('BUDGET/FRAGMENTATION/VALID-MUTATION TESTS AND PUBLIC LEGACY COUNTS ADDED')
