from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');P=R/'src/paper_r7';S=R/'reports/R7-MLE-FOLD-20261108-v1'
assert 'folded K capacity was not released' in (S/'K-RED.log').read_text()
p=P/'public_k.hpp';s=p.read_text().replace('std::uint64_t builder_peak_bytes=0;', 'std::uint64_t builder_peak_bytes=0,ram_compactions=0,ram_released_bytes=0,compaction_copy_bytes=0;\n double compaction_seconds=0;');p.write_text(s)
p=P/'public_k.cpp';s=p.read_text().replace('bool ended=false,failed=false;', 'bool ended=false,failed=false,compact=true;')
s=s.replace('  peak();\n }','''  if(const auto*v=std::getenv("ZKGPT_K_COMPACT")){std::string text(v);need(text=="0" || text=="1","K compaction switch");compact=text=="1";}
  peak();
 }''',1)
old='''   }root.size=old/2;
  }point.push_back(r);'''
new='''   }root.size=old/2;
   if(compact && !root.file && root.ram.capacity()>root.ram.size()){
    auto begin=Clock::now();auto old_bytes=root.ram.capacity()*sizeof(Fr);std::vector<Fr>smaller(root.ram.begin(),root.ram.end());
    auto new_bytes=smaller.capacity()*sizeof(Fr);need(new_bytes<=old_bytes,"K compaction capacity grew");peak(new_bytes);
    root.ram.swap(smaller);ram_bytes-=old_bytes-new_bytes;++metrics.ram_compactions;metrics.ram_released_bytes+=old_bytes-new_bytes;metrics.compaction_copy_bytes+=new_bytes;metrics.compaction_seconds+=seconds(begin);
   }
  }point.push_back(r);'''
assert old in s;s=s.replace(old,new)
s=s.replace('<<",\\"RAM_root_admissions\\":', '<<",\\"ram_compactions\\":"<<m.ram_compactions<<",\\"ram_released_bytes\\":"<<m.ram_released_bytes<<",\\"compaction_copy_bytes\\":"<<m.compaction_copy_bytes<<",\\"compaction_seconds_in_fold_wall\\":"<<m.compaction_seconds<<",\\"RAM_root_admissions\\":')
p.write_text(s);print('K compaction implemented, coefficient generation unchanged')
