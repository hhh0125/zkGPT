from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==R
P=R/'src/paper_r7';S=R/'reports/R7-ROWMAJOR-OPT2-20261028-v1'
p=P/'opt2_metrics.cpp';s=p.read_text();s=s.replace('void* operator new(std::size_t n){if(void*p=std::malloc(n?n:1)){if(tracing)++allocations;return p;}throw std::bad_alloc();}', 'void* operator new(std::size_t n){for(;;){if(void*p=std::malloc(n?n:1)){if(tracing)++allocations;return p;}auto handler=std::get_new_handler();if(!handler)throw std::bad_alloc();handler();}}');s=s.replace('C++ new inside traced wrappers; excludes C malloc and caller construction','ordinary C++ new on tracing thread inside wrappers; excludes caller construction, C malloc, aligned and worker allocation');p.write_text(s)
p=P/'classified_commitments.cpp';s=p.read_text().replace('rootTiming("partial-kernel-dot",roots_[k].spec.id,rootStarted);','rootTiming("partial-exchange",roots_[k].spec.id,rootStarted);');p.write_text(s)
p=P/'opt2_sumcheck_tests.cpp';s=p.read_text().replace('unsigned rounds=0;auto start=','unsigned rounds=0;double polynomialCPU=0;auto start=');s=s.replace('current/=2){auto got=','current/=2){auto polyStarted=processCPU();auto got=');s=s.replace(':reference(e,current,f,folded,current==n);if(!bench)',':reference(e,current,f,folded,current==n);polynomialCPU+=processCPU()-polyStarted;if(!bench)');s=s.replace('<<",\\\"cpu_seconds\\\":"<<processCPU()-cpu','<<",\\\"polynomial_cpu_seconds\\\":"<<polynomialCPU<<",\\\"cpu_seconds\\\":"<<processCPU()-cpu');p.write_text(s)
# Separate Fr weight product microbench, without clocks in the hot at() loop.
p=P/'opt2_kernel_tests.cpp';s=p.read_text();needle='  std::cout<<"{\\"test\\":\\"kernel\\"'
# Actual spelling is located by a stable preceding line rather than escaped C++.
needle='  std::cout<<"{'
at=s.index(needle,s.index(' static void run('));extra=r'''  if(!bench){std::vector<Fr>q(24,Fr(7));auto t=processCPU();Commitments::Weights w(q);auto precompute=processCPU()-t;Fr sum=0;t=processCPU();for(std::size_t i=0;i<(1u<<20);++i)sum+=w.at(i);need(sum.isValid(),"weight arithmetic");std::cout<<"{\"test\":\"weights\",\"precompute_cpu_seconds\":"<<precompute<<",\"at_products\":1048576,\"Fr_multiply_add_cpu_seconds\":"<<processCPU()-t<<",\"cache_bytes\":"<<w.bytes()<<"}\n";}
''';s=s[:at]+extra+s[at:];p.write_text(s)
p=S/'run.py';s=p.read_text();needle='else:raise ValueError(mode)'
s=s.replace(needle,r'''elif mode=='final-checks':
 targets=['r7_opt2_kernel_tests','r7_opt2_sumcheck_tests','r7_classified_crypto_tests','r7_classified_layout_tests','r7_a3_tests','r7_fs_io_tests','r7_oneblock_vectors_tests','r7_oneblock_model_tests','r7_oneblock_lookup_tests','r7_oneblock_gkr_tests','r7_fs_probe_prover','r7_fs_probe_verifier','r7_fs_oneblock_prover','r7_fs_oneblock_verifier','r7_complete_prover','r7_complete_verifier']
 run('final-build',['cmake','--build','build/release','--target',*targets,'-j4'],timeout=1800)
 for target in targets[:10]:run('final-unit-'+target,[B/target])
 for repeat in range(7):
  for implementation in (['reference','optimized'] if repeat%2==0 else ['optimized','reference']):run('polynomial-pair-'+str(repeat)+'-'+implementation,[B/'r7_opt2_sumcheck_tests',implementation])
 for role in ['prover','verifier']:run('final-small-'+role,[B/('r7_fs_probe_'+role),64,'grouped',S/'final-small.fsproof','zkGPT/R7/FS/test-request-20261028'])
'''+needle)
p.write_text(s)
print('FINAL DIRECTED CHECKS READY')
