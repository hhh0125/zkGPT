from pathlib import Path
import os,subprocess
r=Path.cwd();d=r/'reports/R6B-OPT2-20261021';l=r/'logs/R6B-OPT2-20261021'
p=r/'src/prover.cpp';s=p.read_text();a=s.index('        using namespace zkgpt::memory_diagnostics;\n        logAddress("shared_commit/published"');b=s.index('\n    }',a)
s=s[:a]+'''        // Publication is complete. Optional diagnostics must never turn a
        // successful commit into an exception after replacing borrowed views.
        try {
            using namespace zkgpt::memory_diagnostics;
            if(enabled()) {
                logAddress("shared_commit/published","cc.w",cc.w,padded_len*sizeof(Fr),"prover.vector");
                logAddress("shared_commit/published","val0",val[0].data(),val[0].capacity()*sizeof(Fr),"prover.val0");
                logVector("shared_commit/published","committed_vector",committed_witness_vector_);
                logProcess("shared_commit/published");
            }
        } catch (...) { /* A diagnostic allocation cannot invalidate commit success. */ }
        return;'''+s[b:]
s=s.replace('logScalar(stage,"committed_witness_alive",bool(committed_witness_));','logScalar(stage,"committed_witness_alive",bool(committed_witness_) || !committed_witness_vector_.empty());\n    logScalar(stage,"committed_legacy_array_alive",bool(committed_witness_));\n    logScalar(stage,"committed_shared_vector_alive",!committed_witness_vector_.empty());\n    logVector(stage,"committed_shared_vector",committed_witness_vector_);')
p.write_text(s)
p=r/'src/main_opt2_allocation_tests.cpp';s=p.read_text();s=s.replace('std::size_t threshold=1024;','std::size_t threshold=1024;Fr** watched=nullptr;Fr* previous=nullptr;bool reject_published=false;std::atomic<unsigned> diagnostic_failures{0};')
s=s.replace('if(track){if(fail==0)', 'if(track){if(reject_published && watched && *watched!=previous){++diagnostic_failures;throw std::bad_alloc();}if(fail==0)')
s=s.replace(' std::cout<<"PASS actualoneMasterrequestremoved, samevalues, vector/rowsfailuresatomic\\n";', ''' setenv("ZKGPT_MEMORY_DIAGNOSTICS","1",1);watched=&p.cc.w;previous=p.cc.w;reject_published=true;track=true;
 p.commitInput(p.gens,1,true);track=false;reject_published=false;unsetenv("ZKGPT_MEMORY_DIAGNOSTICS");
 check(diagnostic_failures>0 && p.cc.w!=previous && p.sharedInputWitness()==copied,"postpublishdiagnosticfailureinvalidatedsuccess");
 std::cout<<"PASS actualoneMasterrequestremoved, samevalues, vector/rowsfailuresatomic, postpublicationdiagnosticbadallocsuppressed\\n";''')
p.write_text(s)
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
with (l/'review-fix-build.log').open('x') as f:subprocess.run(['cmake','--build','build/release','-j','4'],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=420)
with (l/'allocation-review-fixed.log').open('x') as f:subprocess.run(['build/release/src/opt2_allocation_tests'],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=120)
print((l/'allocation-review-fixed.log').read_text()[-1600:])
with (d/'PROGRESS.md').open('a') as f:f.write('Read-onlyreview Important fixed: publishedcommit diagnostic errors now suppressed, disableddiagnostics avoidlabelallocations. Newpostpublicationbadalloc fault test confirmscommitreturns successwithvalidnewowner. Minor metricfixed: legacy/sharedowner flags+capacity explicit. Rebuild finalbinaries, repeatrealoneblockforfinalidentity; arithmetic/transcript unchanged.\n')
