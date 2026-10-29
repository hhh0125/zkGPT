from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==R
P=R/'src/paper_r7'
p=P/'classified_commitments.cpp';s=p.read_text();s=s.replace('<<"}\\n";return result;','<<"}\\n";\n return result;').replace('<<"}\\n";return values;','<<"}\\n";\n return values;');p.write_text(s)
# Keep reference implementation available only to the directed test friend.
p=P/'opt2_kernel_tests.cpp';s=p.read_text();start=s.index(' static void run(');end=s.index('\n};',start)
s=s[:start]+r''' static void run(Commitments&c,bool bench,bool optimized){
  std::vector<Fr>lambda;for(unsigned i=0;i<c.claims_.size();++i)lambda.push_back(Fr(i+2));
  auto start=std::chrono::steady_clock::now();auto cpu=processCPU();Fr checksum=0,bias=0;
  Commitments::Authentication prepared(c.claims_.size());
  if(optimized)for(unsigned i=0;i<c.claims_.size();++i)bias+=lambda[i]*c.prepareClaim(c.claims_[i],prepared,i);
  else for(unsigned i=0;i<c.claims_.size();++i){auto&a=c.claims_[i];need(c.mapDigest(a)==a.map_digest,"reference barrier map");if(a.only_root<0){auto w=r1::weights(a.point);for(std::size_t j=0;j<a.padded;++j)bias+=lambda[i]*w[j]*a.map(j).bias;}}
  std::vector<Fr>dots(c.roots_.size(),Fr(0));
  if(optimized)dots=c.partialDot(lambda,prepared);
  else for(unsigned k=0;k<6;++k){auto kernel=c.kernel(k,lambda);for(std::size_t j=0;j<kernel.size();++j)dots[k]+=kernel[j]*c.roots_[k].value(j);}
  for(unsigned k=0;k<6;++k){auto got=optimized?c.kernel(k,lambda,prepared):c.kernel(k,lambda);if(!bench){auto ref=c.kernel(k,lambda);need(got==ref,"optimized/reference kernel mismatch");std::vector<Fr>want(got.size(),Fr(0));for(unsigned i=0;i<c.claims_.size();++i){auto&a=c.claims_[i];auto w=r1::weights(a.point);for(std::size_t j=0;j<a.padded;++j){auto l=a.map(j);if(!l.constant && l.coordinate.root==k)want[l.coordinate.index]+=lambda[i]*w[j];}}need(want==got,"kernel dense reference mismatch");Fr dot=0;for(std::size_t j=0;j<got.size();++j)dot+=got[j]*c.roots_[k].value(j);need(dot==dots[k],"partial-dot reference mismatch");}checksum+=got[0]+dots[k];}
  // A validated snapshot is independent of later mutations of callbacks.
  if(!bench && optimized){auto good=c.kernel(0,lambda,prepared);c.claims_[0].map=[](std::size_t)->Located{throw std::runtime_error("callback must not be reused after freeze");};need(good==c.kernel(0,lambda,prepared),"snapshot retained mutable callback");
   need(prepared.map_bytes<=Commitments::Authentication::map_limit && prepared.weight_bytes<=Commitments::Authentication::weight_limit,"cache budget exceeded");
   // Force the generic checked path and ensure it detects a changed callback.
   prepared.claims[0].frozen=false;bool rejected=false;try{c.kernel(0,lambda,prepared);}catch(...){rejected=true;}need(rejected,"fallback mutable map accepted");}
  std::cout<<"{\"test\":\"kernel\",\"mode\":\""<<(optimized?"optimized":"reference")<<"\",\"n\":"<<c.roots_[0].spec.padded<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"cpu_seconds\":"<<processCPU()-cpu<<",\"checksum\":\""<<(checksum+bias).getStr()<<"\"}\n";
 }'''+s[end:]
s=s.replace('bool bench=argc>1 && std::string(argv[1])=="bench";','bool bench=argc>1;bool optimized=argc<=1 || std::string(argv[1])!="reference";').replace('AuthenticationTestAccess::run(c,bench);','AuthenticationTestAccess::run(c,bench,optimized);');p.write_text(s)
# Keep allocation-only alternative in the benchmark until measurements justify it.
p=P/'opt2_msm_bench.cpp';s=p.read_text().replace('need(mode<3','need(mode<4');s=s.replace('}else{g.mul(x,v);h.mul(y,r);G1::add(out,x,y);}', '}else if(m==2){g.mul(x,v);h.mul(y,r);G1::add(out,x,y);}else{std::array<G1,2>bases{p.scalar,p.hiding};std::array<Fr,2>s{v,r};G1::mulVec(out,bases.data(),s.data(),2);}')
s=s.replace('evaluate(0,a[i],b[i])==evaluate(2,a[i],b[i])','evaluate(0,a[i],b[i])==evaluate(2,a[i],b[i]) && evaluate(0,a[i],b[i])==evaluate(3,a[i],b[i])');p.write_text(s)
p=P/'hyrax_a3.cpp';s=p.read_text().replace('void opening(IO&io,const PC&pc', '// Legacy bounded ColumnMajor PC API. Current RowMajor Classified FS and\n// Interactive controllers call dotProduct / dotProductTrustedR6Basis through\n// openVector and model::opening. crypto::validate rejects any other layout tag.\nvoid opening(IO&io,const PC&pc');p.write_text(s)
print('BRACES, CONTROLLED HARNESS AND LEGACY BOUNDARY UPDATED')
