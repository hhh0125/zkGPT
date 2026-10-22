#pragma once
#include "lookup_batch.hpp"
// Deliberately dishonest, TEST-ONLY prover. Produces genuine commitments and
// openings but refuses to check that the Sumcheck's initial sum equals the
// authenticated local claims. The real verifier must detect that mismatch.
inline zkgpt::r1::MappedInputProof hostileMappedInput(
 const zkgpt::r1::MappedInputSpec &s,const std::vector<zkgpt::r1::Fr> &w,
 const std::vector<std::vector<zkgpt::r1::Fr>> &v,zkgpt::r1::Fs &t,
 const std::vector<zkgpt::r1::CheckedInputClaim> &checked={}) {
 using namespace zkgpt::r1;appendMappedInput(s,t);appendCheckedInput(s,checked,t);MappedInputProof p;
 std::vector<std::vector<Fr>> points;
 for(std::size_t j=0;j<s.children.size();++j)points.push_back(point(t,log2exact(s.children[j].commitment.spec.padded),"mapped.child.point/"+std::to_string(j)));
 for(std::size_t j=0;j<s.children.size();++j){const auto &c=s.children[j].commitment;p.children.push_back(open(c.spec,c,v[j],points[j],t,"mapped.child/"+std::to_string(j)));}
 std::vector<Fr>k(s.master.spec.padded,Fr(0));
 for(std::size_t j=0;j<s.children.size();++j){auto lambda=t.challenge("mapped.lambda/"+std::to_string(j));auto weights_j=weights(points[j]);std::size_t i=0;for(const auto &span:s.children[j].spans)for(std::size_t z=0;z<span.count;++z)k[span.offset+z]+=lambda*weights_j[i++];}
 for(std::size_t j=0;j<s.public_values.size();++j)k[s.public_values[j].index]+=t.challenge("mapped.public.lambda/"+std::to_string(j));
 for(std::size_t j=0;j<checked.size();++j){auto lambda=t.challenge("mapped.checked.lambda/"+std::to_string(j));auto weight=weights(checked[j].point);std::size_t i=0;for(const auto &span:checked[j].spans)for(std::size_t z=0;z<span.count;++z)k[span.offset+z]+=lambda*weight[i++];}
 auto tail=weights(point(t,log2exact(s.master.spec.padded),"mapped.master.tail"));auto lambda=t.challenge("mapped.tail.lambda");
 for(std::size_t i=s.master.spec.actual;i<k.size();++i)k[i]+=lambda*tail[i];
 auto end=proveSumcheck({k,w},2,[](const auto &f){return f[0]*f[1];},p.sumcheck,t,"mapped.input");
 p.master=open(s.master.spec,s.master,w,end.point,t,"mapped.master");return p;
}
inline zkgpt::r1::LookupBatchProof hostileLookupBatch(
 const zkgpt::r1::LookupBatchSpec &s,const std::vector<zkgpt::r1::Fr> &w,
 const std::vector<std::vector<zkgpt::r1::Fr>> &v) {
 using namespace zkgpt::r1;Fs t("lookup");appendLookupBatch(s,t);LookupBatchProof p;
 for(std::size_t j=0;j<s.ranges.size();++j){auto k=s.ranges[j].child;p.ranges.push_back(proveRange(rangeSpec(s,j),s.input.children[k].commitment,v[k],t));}
 for(std::size_t j=0;j<s.exps.size();++j){const auto &e=s.exps[j];p.exps.push_back(proveExp(expSpec(s,j),s.input.children[e.index_child].commitment,s.input.children[e.value_child].commitment,v[e.index_child],v[e.value_child],t));}
 // Prove that every local lookup would be accepted, before constructing the
 // inconsistent input proof. This does not call the input verifier.
 Fs check("lookup");appendLookupBatch(s,check);
 for(std::size_t j=0;j<s.ranges.size();++j)verifyRange(rangeSpec(s,j),s.input.children[s.ranges[j].child].commitment,p.ranges[j],check);
 for(std::size_t j=0;j<s.exps.size();++j){const auto &e=s.exps[j];verifyExp(expSpec(s,j),s.input.children[e.index_child].commitment,s.input.children[e.value_child].commitment,p.exps[j],check);}
 require(check.digest()==t.digest(),"hostile fixture local lookup transcript");
 p.input=hostileMappedInput(s.input,w,v,t);return p;
}
