p=Path('src/paper_r7/full12_statement.cpp');s=p.read_text();p.write_text(s.replace('#include "paper_r1/wire.hpp"','#include "paper_r1/wire.hpp"\n#include "paper_r2/r5b_trust.hpp"'))
p=Path('src/paper_r7/oneblock_main.cpp');s=p.read_text();old='auto t=std::chrono::steady_clock::now();auto bytes=pipe.proofBytes(),calls=metrics().msm_calls;';assert old in s;s=s.replace(old,'auto t=std::chrono::steady_clock::now();[[maybe_unused]] auto cpuStarted=processCPU();auto bytes=pipe.proofBytes(),calls=metrics().msm_calls;');old='std::cerr<<"PHASE_COMPLETE "<<id<<\'\\n\';';assert old in s;s=s.replace(old,'''std::cerr<<"PHASE_COMPLETE "<<id<<'\\n';
#ifdef R7_FS_FULL12
 std::cerr<<"PHASE_CPU {\\\"name\\\":\\\""<<id<<"\\\",\\\"cpu_seconds\\\":"<<processCPU()-cpuStarted<<"}\\n";
#endif
''');p.write_text(s)
print('FIXED_STATEMENT_INCLUDE_AND_ADDED_FULL12_PHASE_CPU_METRICS')
