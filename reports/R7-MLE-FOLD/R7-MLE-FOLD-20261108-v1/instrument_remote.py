from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');P=R/'src/paper_r7'
p=P/'unified_input.hpp';s=p.read_text();s=s.replace('Value read={};};','Value read={};const void*storage_identity=nullptr;std::size_t storage_offset=0;};')
s=s.replace('double scan_cpu=0,scan_wall=0,product_wall=0;', 'std::uint64_t scan_calls=0,read_elements=0,chunk_reads=0,buffer_hits=0,buffer_misses=0,state_hits=0,state_misses=0;\n double read_wall=0,read_cpu=0,weights_wall=0,scan_cpu=0,scan_wall=0,product_wall=0;')
p.write_text(s)
p=P/'u_streaming.cpp';s=p.read_text();s=s.replace('#include <mutex>','#include <mutex>\n#include <time.h>')
s=s.replace('using Clock=std::chrono::steady_clock;', 'using Clock=std::chrono::steady_clock;\ndouble threadCPU(){timespec t{};need(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t)==0,"thread CPU clock");return t.tv_sec+t.tv_nsec*1e-9;}')
s=s.replace('Workers workers(inputs && !cached?options.threads:1);','Workers workers(inputs && !cached?options.threads:1);std::array<UMetrics,4>reads{};')
s=s.replace('auto&owned=weights[low];if(!owned){','auto&owned=weights[low];if(!owned){auto weight_start=Clock::now();')
s=s.replace('weight_bytes+=owned->bytes();}', 'weight_bytes+=owned->bytes();m.weights_wall+=elapsed(weight_start);}')
s=s.replace('auto read=[&](std::size_t index)->Fr{return inputs?(index<span.stored?(span.data?span.data[index]:span.read(index)):Fr(0)):a(index);};','')
s=s.replace('auto lo=block*shard/shards,hi=block*(shard+1)/shards;', '''auto lo=block*shard/shards,hi=block*(shard+1)/shards;
     auto&rm=reads[job%options.threads];++rm.scan_calls;
     std::array<Fr,1024>read_buffer;std::size_t buffer_begin=0,buffer_count=0;
     auto read=[&](std::size_t index)->Fr{
      if(!inputs)return a(index);if(index>=span.stored)return Fr(0);
      if(span.data){++rm.read_elements;return span.data[index];}
      if(index<buffer_begin || index-buffer_begin>=buffer_count){
       auto started=Clock::now();auto cpu=threadCPU();buffer_begin=index;buffer_count=std::min<std::size_t>(read_buffer.size(),span.stored-index);
       for(std::size_t z=0;z<buffer_count;++z)read_buffer[z]=span.read(index+z);
       rm.read_wall+=elapsed(started);rm.read_cpu+=threadCPU()-cpu;rm.read_elements+=buffer_count;++rm.chunk_reads;++rm.buffer_misses;
      }else ++rm.buffer_hits;
      return read_buffer[index-buffer_begin];
     };''')
s=s.replace('if(cached){tile[job]=span.data[first+local];return;}', 'if(cached){tile[job]=span.data[first+local];++rm.read_elements;return;}')
s=s.replace('if(metrics)*metrics=m;return out;', '''for(const auto&r:reads){m.scan_calls+=r.scan_calls;m.read_elements+=r.read_elements;m.chunk_reads+=r.chunk_reads;m.buffer_hits+=r.buffer_hits;m.buffer_misses+=r.buffer_misses;m.read_wall+=r.read_wall;m.read_cpu+=r.read_cpu;}
 if(metrics)*metrics=m;return out;''')
# A/B use exactly the same bounded instrumentation. No FS messages added.
p.write_text(s)
p=P/'unified_input.cpp';s=p.read_text();anchor='   if(crypto::opt2MetricsEnabled())std::cerr<<"OPT3D_U_ROUND '
pos=s.index(anchor);s=s[:pos]+'''   if(crypto::opt2MetricsEnabled())std::cerr<<"U_SCAN_DETAIL {\\"round\\":"<<t<<",\\"u_scan_call_count\\":"<<m.scan_calls<<",\\"u_scan_total_elements\\":"<<m.read_elements<<",\\"u_scan_total_bytes\\":"<<m.read_elements*sizeof(Fr)<<",\\"u_scan_wall_time\\":"<<m.scan_wall<<",\\"u_scan_cpu_time\\":"<<m.scan_cpu<<",\\"chunk_read_count\\":"<<m.chunk_reads<<",\\"chunk_read_bytes\\":"<<m.read_elements*sizeof(Fr)<<",\\"chunk_cache_hit\\":"<<m.buffer_hits<<",\\"chunk_cache_miss\\":"<<m.buffer_misses<<",\\"coordinate_mapping_time_including_source_read_worker_wall\\":"<<m.read_wall<<",\\"coordinate_mapping_cpu_including_source_read\\":"<<m.read_cpu<<",\\"scan_cpu_minus_read_cpu_including_Fr_and_dispatch\\":"<<m.scan_cpu-m.read_cpu<<",\\"mle_weights_wall\\":"<<m.weights_wall<<",\\"u_state_cache_hit\\":"<<m.state_hits<<",\\"u_state_cache_miss\\":"<<m.state_misses<<",\\"physical_disk_U_bytes\\":0}\\n";
'''+s[pos:];p.write_text(s)
p=P/'category_auth.cpp';s=p.read_text();s=s.replace('auto plan=categoryTiles(layout_);','auto tile_started=std::chrono::steady_clock::now();auto plan=categoryTiles(layout_);\n std::cerr<<"CATEGORY_TILE_DETAIL {\\"tile_enumeration_count\\":1,\\"tiles\\":"<<plan.size()<<",\\"tile_mapping_time\\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-tile_started).count()<<"}\\n";')
s=s.replace('s.stored=s.padded=t.segment.padded;s.read=', 's.stored=s.padded=t.segment.padded;s.storage_identity=&roots_[t.root];s.storage_offset=t.begin;s.read=');p.write_text(s)
print('common A/B instrumentation installed')
