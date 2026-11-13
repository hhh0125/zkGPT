from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');P=R/'src/paper_r7';S=R/'reports/R7-MLE-FOLD-20261108-v1'
assert 'error:' in (S/'review-metrics-RED.log').read_text()
p=P/'unified_input.hpp';s=p.read_text().replace('chunk_reads=0,buffer_hits=0,buffer_misses=0,','profile_refills=0,profile_hits=0,profile_misses=0,original_read_elements=0,retained_read_elements=0,storage_chunks_touched=0,')
s=s.replace('double read_wall=0,','bool storage_identity_complete=true,read_profile_enabled=false;\n double read_wall=0,')
s=s.replace('Round quadraticRoundStreaming(', 'std::string scanMetricsJSON(unsigned,const UMetrics&);\nRound quadraticRoundStreaming(');p.write_text(s)
p=P/'u_streaming.cpp';s=p.read_text().replace('#include <time.h>','#include <time.h>\n#include <set>\n#include <sstream>')
s=s.replace('UMetrics m;Round out;', 'UMetrics m;m.read_profile_enabled=options.profile_reads;std::set<const void*>source_chunks;Round out;')
s=s.replace('if(inputs){if(cached)++m.state_hits;else ++m.state_misses;}', 'if(inputs){if(cached)++m.state_hits;else{++m.state_misses;if(span.stored){if(span.storage_identity)source_chunks.insert(span.storage_identity);else m.storage_identity_complete=false;}}}')
s=s.replace('++rm.read_elements;return span.data[index];', '++rm.read_elements;++rm.original_read_elements;return span.data[index];')
s=s.replace('++rm.read_elements;++rm.mapping_calls;return span.read(index);', '++rm.read_elements;++rm.original_read_elements;++rm.mapping_calls;return span.read(index);')
s=s.replace('rm.read_elements+=buffer_count;', 'rm.read_elements+=buffer_count;rm.original_read_elements+=buffer_count;')
s=s.replace('++rm.chunk_reads;++rm.buffer_misses;', '++rm.profile_refills;++rm.profile_misses;').replace('++rm.buffer_hits;', '++rm.profile_hits;')
s=s.replace('++rm.read_elements;return;}', '++rm.read_elements;++rm.retained_read_elements;return;}')
s=s.replace('m.chunk_reads+=r.chunk_reads;m.buffer_hits+=r.buffer_hits;m.buffer_misses+=r.buffer_misses;', 'm.profile_refills+=r.profile_refills;m.profile_hits+=r.profile_hits;m.profile_misses+=r.profile_misses;m.original_read_elements+=r.original_read_elements;m.retained_read_elements+=r.retained_read_elements;')
s=s.replace('if(metrics){*metrics=m;}', 'm.storage_chunks_touched=source_chunks.size();if(metrics){*metrics=m;}')
insert=r'''
std::string scanMetricsJSON(unsigned round,const UMetrics&m){std::ostringstream out;
 out<<"{\"round\":"<<round<<",\"u_scan_call_count\":"<<m.scan_calls<<",\"u_scan_total_elements\":"<<m.read_elements<<",\"u_scan_total_bytes\":"<<m.read_elements*sizeof(Fr)<<",\"original_source_read_elements\":"<<m.original_read_elements<<",\"original_source_scalar_payload_bytes\":"<<m.original_read_elements*sizeof(Fr)<<",\"retained_state_read_elements\":"<<m.retained_read_elements<<",\"retained_state_payload_bytes\":"<<m.retained_read_elements*sizeof(Fr)<<",\"u_scan_wall_time\":"<<m.scan_wall<<",\"u_scan_cpu_time\":"<<m.scan_cpu<<",\"coordinate_mapping_calls\":"<<m.mapping_calls<<",\"original_storage_chunks_touched\":";
 if(m.storage_identity_complete)out<<m.storage_chunks_touched;else out<<"null";
 out<<",\"storage_identity_complete\":"<<(m.storage_identity_complete?"true":"false")<<",\"chunk_read_count\":null,\"chunk_read_bytes\":null,\"chunk_cache_hit\":null,\"chunk_cache_miss\":null,\"physical_chunk_metrics\":\"NOT_MEASURED_NO_BULK_CHUNK_API\",\"source_chunk_cache\":\"NOT_APPLICABLE_RESIDENT_SCALAR_READERS\",\"read_profile_enabled\":"<<(m.read_profile_enabled?"true":"false")<<",\"profile_buffer_refill_count\":"<<m.profile_refills<<",\"profile_buffer_payload_bytes\":"<<m.timed_read_elements*sizeof(Fr)<<",\"profile_buffer_hit\":"<<m.profile_hits<<",\"profile_buffer_miss\":"<<m.profile_misses<<",\"timed_original_source_elements\":"<<m.timed_read_elements<<",\"profiled_read_mapping_worker_wall\":"<<m.read_wall<<",\"profiled_read_mapping_worker_cpu\":"<<m.read_cpu<<",\"scan_cpu_minus_profiled_read_cpu_includes_untimed_read_Fr_dispatch\":"<<m.scan_cpu-m.read_cpu<<",\"mle_weights_wall\":"<<m.weights_wall<<",\"u_state_cache_hit\":"<<m.state_hits<<",\"u_state_cache_miss\":"<<m.state_misses<<",\"U_explicit_file_IO_bytes\":0}";return out.str();}
'''
pos=s.rfind('\n}');s=s[:pos]+insert+s[pos:];p.write_text(s)
p=P/'unified_input.cpp';s=p.read_text();start=s.index('   if(crypto::opt2MetricsEnabled())std::cerr<<"U_SCAN_DETAIL ');end=s.index('\n',start);s=s[:start]+'   if(crypto::opt2MetricsEnabled())std::cerr<<"U_SCAN_DETAIL "<<scanMetricsJSON(t,m)<<"\\n";'+s[end:];p.write_text(s)
print('review Important fixed: physical unknown != profile buffer != retained state')
