from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');P=R/'src/paper_r7';S=R/'reports/R7-MLE-FOLD-20261108-v1'
source=(R/'reports/R7-CATEGORY-X-20261106-v1/category_kernel_clipping_probe.cpp').read_text()
start=source.index(' static void clipping(');end=source.index('\n static void mutableFallback',start)
method=r''' static void endpoints(Commitments&pcs,Layout&layout){std::mt19937 random(20261108);
  auto chi=[](std::size_t z,const std::vector<Fr>&q){Fr w=1;for(std::size_t b=0;b<q.size();++b)w*=((z>>b)&1)?q[b]:Fr(1)-q[b];return w;};
  for(unsigned trial=0;trial<220;++trial){
   std::array<std::size_t,5>a{};for(auto&n:a)n=1+random()%256;if(trial<5)a={64,256,129,33,65};auto plan=single::makeCategoryPlan(a,std::string(64,'a'));const char*names[]{"X","W","Aux","LookupQueries","LookupHelpers"};
   layout.roots.clear();pcs.roots_.resize(5);for(auto s:plan.segments){unsigned k=0;while(s.source_id!=names[k])++k;layout.categories[k]={s.actual,s.padded,s.offset,s.physical_offset};}
   for(unsigned k=0;k<5;++k){RootSpec spec{names[k],a[k],a[k],k!=4,k==1,k,0};layout.roots.push_back(spec);pcs.roots_[k].spec=spec;}
   pcs.claims_.clear();pcs.names_.clear();std::vector<std::shared_ptr<std::vector<Located>>>maps;
   for(unsigned claim=0;claim<2;++claim){auto locations=std::make_shared<std::vector<Located>>(256);maps.push_back(locations);
    for(std::size_t j=0;j<256;++j){Located l;l.constant=true;l.bias=Fr(int(j%3)-1);unsigned root=1;std::size_t index=0;
     switch(trial%5){case 0:index=j;l.constant=j>=a[root];break;case 1:index=j<a[root]?a[root]-1-j:0;l.constant=j>=a[root];break;case 2:root=trial%5;index=std::min<std::size_t>(3,a[root]-1);l.constant=false;break;case 3:index=j/2;l.constant=(j%2 || index>=a[root]);break;default:root=(j/16)%5;index=j%a[root];l.constant=false;}
     l.coordinate={root,std::uint32_t(index)};(*locations)[j]=l;
    }
    Claim c;c.id="endpoint/"+std::to_string(trial)+"/"+std::to_string(claim);c.map_id=c.id;c.padded=256;for(unsigned j=0;j<8;++j)c.point.emplace_back(trial%3?int(random()%17)-8:int(random()%2));c.value=publicHidden(pcs.io_,c.id,Fr(0));c.map=[locations](std::size_t j){return locations->at(j);};pcs.defer(c);
   }
   Commitments::Authentication auth(2,false);for(unsigned j=0;j<2;++j)pcs.prepareClaim(pcs.claims_[j],auth,j);
   std::vector<Fr>rho{Fr(3),Fr(7)},q(single::bits(plan.domain));for(auto&v:q)v=trial%3?Fr(int(random()%19)-9):Fr(random()%2);
   Fr reference=0;for(unsigned i=0;i<2;++i)for(std::size_t j=0;j<256;++j){auto l=maps[i]->at(j);if(l.constant)continue;auto z=layout.categories[l.coordinate.root].logical_offset+l.coordinate.index;reference+=rho[i]*chi(j,pcs.claims_[i].point)*chi(z,q);}
   auto direct=pcs.categoryKernelEndpoint(q,rho,auth,false);need(direct && *direct==reference,"direct global K endpoint != independent source/global bit products");
   Fr dense=0;for(unsigned root=0;root<5;++root){auto values=pcs.categoryKernel(root,0,a[root],rho,auth);for(std::size_t j=0;j<values.size();++j)dense+=values[j]*chi(layout.categories[root].logical_offset+j,q);}need(dense==reference,"dense K reference mismatch");
   // The callback fallback must retain the exact post-rho map digest check.
   for(auto&p:auth.claims){p.frozen=false;p.runs.clear();p.disk_roots.clear();}
   direct=pcs.categoryKernelEndpoint(q,rho,auth,false);need(direct && *direct==reference,"fallback K endpoint differs");
   auto&location=maps[0]->at(0);location.bias+=Fr(1);bool rejected=false;try{pcs.categoryKernelEndpoint(q,rho,auth,false);}catch(const std::exception&e){rejected=std::string(e.what()).find("source map changed")!=std::string::npos;}need(rejected,"endpoint fallback ignored mutated map");
  }
  std::cout<<"CATEGORY_GLOBAL_K_ENDPOINT_220_STRIDES_STEPS_REPEATS_BIASES_CROSS_CATEGORY_AND_MAP_MUTATION_PASS\n";
 }
'''
source=source[:start]+method+source[end:];source=source.replace('#include <iostream>','#include <iostream>\n#include <random>')
# Retain the original real protocol/negative tests, then add the pure public-K
# reference exercise to a separate instance. No fake layout reaches acceptance.
source=source.replace('AuthenticationTestAccess::clipping(pcs);','AuthenticationTestAccess::endpoints(pcs,l);')
(P/'category_k_endpoint_tests.cpp').write_text(source)
p=P/'CMakeLists.txt';p.write_text(p.read_text()+'''
add_executable(r7_category_k_endpoint_tests category_k_endpoint_tests.cpp)
target_link_libraries(r7_category_k_endpoint_tests PRIVATE paper_r7)
add_test(NAME r7_category_k_endpoint COMMAND r7_category_k_endpoint_tests)
set_tests_properties(r7_category_k_endpoint PROPERTIES TIMEOUT 180 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} ENVIRONMENT "ZKGPT_THREADS=4;MALLOC_ARENA_MAX=2")
''')
