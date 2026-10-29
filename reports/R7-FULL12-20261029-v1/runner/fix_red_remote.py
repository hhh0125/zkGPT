import subprocess
p=Path('src/paper_r7/full12_plan_tests.cpp');p.write_text(p.read_text().replace(' r1::init();',' zkgpt::r1::init();'))
stage=Path('reports/R7-FULL12-20261029-v1')
with (stage/'plan-red-corrected.log').open('wb') as out:r=subprocess.run(['cmake','--build','build/release','--target','r7_full12_plan_tests','-j4'],stdout=out,stderr=subprocess.STDOUT)
log=(stage/'plan-red-corrected.log').read_text();assert r.returncode!=0 and 'too many arguments' in log and 'has not been declared' not in log
print(log[-2100:])
