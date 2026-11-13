from pathlib import Path
import subprocess,shlex,os
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(R);S=R/'reports/R7-MLE-FOLD-20261108-v1';B=R/'build/release/r7-mle-fold-20261108-v1';P=R/'src/paper_r7'
source=S/'callback_lifetime_regression.cpp';source.write_text(r'''#include "u_fold_state.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;namespace ui=oneblock::unified;
int main(){try{r1::init();std::vector<Fr>v(4096,Fr(7));std::weak_ptr<int>weak;std::unique_ptr<ui::FoldedU>state;
 {auto owner=std::make_shared<int>(3);weak=owner;ui::Inputs inputs=[&,owner](std::size_t){ui::InputSpan s;s.stored=s.padded=v.size();s.storage_identity=&v;s.read=[&,owner](std::size_t j){return v.at(j)+Fr(*owner*0);};return s;};state=std::make_unique<ui::FoldedU>(8192,std::vector<ui::Segment>{{0,4096}},inputs,"lifetime",80ull<<10,true);need(state->enabled(),"fixture admission");}
 need(weak.expired(),"cache retained allocating original callback");std::cout<<"CALLBACK_DESCRIPTOR_ONLY_LIFETIME_PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
''')
unit=B/'src/paper_r7/CMakeFiles/r7_category_cache_tests.dir';flags={}
for line in (unit/'flags.make').read_text().splitlines():
 if ' = ' in line:k,v=line.split(' = ',1);flags[k]=v
obj=S/'callback-lifetime.o';args=['c++']+sum((shlex.split(flags.get(k,'')) for k in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']),[])+['-c',str(source),'-o',str(obj)]
p=subprocess.run(args,capture_output=True,text=True);assert p.returncode==0,p.stderr
link=shlex.split((unit/'link.txt').read_text());at=next(i for i,v in enumerate(link) if v.endswith('category_cache_tests.cpp.o'));link[at]=str(obj);out=S/'callback-lifetime-RED';link[link.index('-o')+1]=str(out);p=subprocess.run(link,cwd=B/'src/paper_r7',capture_output=True,text=True);assert p.returncode==0,p.stderr
p=subprocess.run([out],capture_output=True,text=True);(S/'callback-lifetime-RED.log').write_text(p.stdout+p.stderr);assert p.returncode and 'retained allocating' in p.stderr
p=P/'u_fold_state.cpp';s=p.read_text();old='shape(original,s[k].padded,deferred);roots[k].original=original;';new='''shape(original,s[k].padded,deferred);
   // Retain a nonallocating presence marker, never the owning reader closure.
   // The actual immutable reader is borrowed afresh for each round.
   if(deferred && original.read)original.read=[](std::size_t)->Fr{throw std::runtime_error("category U descriptor marker cannot read");};
   roots[k].original=std::move(original);''';assert old in s;s=s.replace(old,new).replace('if(!cached()){return r.original;}','if(!cached()){return original;}');p.write_text(s)
# Pin this ownership/budget property in the registered test as well.
p=P/'category_cache_tests.cpp';s=p.read_text();at=s.index(' ui::FoldedU drift(');case=source.read_text().split('int main(){try{r1::init();',1)[1].split(' std::cout',1)[0] if False else ''
extra=''' {std::weak_ptr<int>weak;std::unique_ptr<ui::FoldedU>descriptor;
 {auto owner=std::make_shared<int>(3);weak=owner;ui::Inputs temporary=[&,owner](std::size_t k){auto out=inputs(k);out.read=[&,k,owner](std::size_t j){return v[k].at(j)+Fr(*owner*0);};return out;};descriptor=std::make_unique<ui::FoldedU>(8192,s,temporary,"owner",80ull<<10,true);}
 need(weak.expired(),"category cache retains caller closure allocation");}
''';s=s[:at]+extra+s[at:];p.write_text(s)
p=P/'classified_controller.inc';s=p.read_text();anchor='  prepared=classified::prepareLookup(ctx,master);';assert anchor in s
s=s.replace(anchor,anchor+r'''
  {std::uint64_t helpers=0,helper_elements=0,layers=0;std::set<const void*>seen;
   for(const auto&entry:prepared)if(entry.second && seen.insert(entry.second.get()).second){helpers+=entry.second->capacity()*sizeof(Fr);helper_elements+=entry.second->size();}
   const auto&values=witness.executionForR7().val;for(std::size_t j=1;j<values.size();++j)layers+=values[j].capacity()*sizeof(Fr);
   std::cerr<<"WITNESS_STORAGE {\"native_master_size\":"<<witness.baseMaster().size()<<",\"native_master_capacity_bytes\":"<<witness.baseMaster().capacity()*sizeof(Fr)<<",\"helper_materialized_elements\":"<<helper_elements<<",\"helper_capacity_bytes\":"<<helpers<<",\"other_native_layer_capacity_bytes\":"<<layers<<",\"snapshot\":\"all-witness-and-helpers-ready\",\"capacity_not_RSS\":true}\n";}
''');p.write_text(s)
print('closure retention RED reproduced and fixed; witness capacities exposed')
