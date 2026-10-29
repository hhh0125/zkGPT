import subprocess,json,hashlib,shlex,os
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1')
source=stage/'anchor-negative-probe.cpp'
source.write_text('''#include "full12_model.hpp"
#include "master_layout.hpp"
#include "paper_r1/wire.hpp"
#include <iostream>
int main(int argc,char**argv){try{if(argc!=2)return 3;zkgpt::r1::init();auto c=zkgpt::r7::master3::trustedContext(12);auto t=zkgpt::r7::full12::loadTrust(c,argv[1]);std::cout<<"TRUST_ACCEPT chunks="<<t.chunks.size()<<"\\n";return 0;}catch(const std::exception&e){std::cerr<<"TRUST_REJECT "<<e.what()<<"\\n";return 2;}}
''')
build=Path('build/release/src/paper_r7');flags={}
for line in (build/'CMakeFiles/r7_full12_anchor_tests.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
obj=Path.cwd()/stage/'anchor-negative-probe.o';binary=Path.cwd()/stage/'anchor-negative-probe'
compile_cmd=['/usr/bin/c++']+flags['CXX_DEFINES']+flags['CXX_INCLUDES']+flags['CXX_FLAGS']+['-c',str(Path.cwd()/source),'-o',str(obj)]
subprocess.run(compile_cmd,check=True)
link=shlex.split((build/'CMakeFiles/r7_full12_anchor_tests.dir/link.txt').read_text())
link[link.index('CMakeFiles/r7_full12_anchor_tests.dir/full12_anchor_tests.cpp.o')]=str(obj)
link[link.index('-o')+1]=str(binary)
subprocess.run(link,cwd=build,check=True)
raw=(stage/'full12-chunks.anchor').read_bytes();at=0
for _ in range(6):
 n=int.from_bytes(raw[at:at+4],'little');at+=4+n
at+=16;count_at=at;count=int.from_bytes(raw[at:at+4],'little');at+=4;records=[]
for _ in range(count):
 start=at;size=int.from_bytes(raw[at+12:at+16],'little');at+=16+size;records.append(raw[start:at])
assert count==10 and at==len(raw)
prefix=raw[:count_at];cases={
 'replaced':raw[:-1]+bytes([raw[-1]^1]),
 'missing':prefix+(9).to_bytes(4,'little')+b''.join(records[:-1]),
 'reordered':prefix+(10).to_bytes(4,'little')+records[1]+records[0]+b''.join(records[2:]),
 'mixed':prefix+(10).to_bytes(4,'little')+records[0]+records[1][:12]+records[0][12:]+b''.join(records[2:])}
results=[]
for name,data in [('fixed-original',raw)]+list(cases.items()):
 p=stage/('anchor-negative-'+name+'.bundle');p.write_bytes(data)
 command=[str(binary),str(p)]
 r=subprocess.run(command,capture_output=True,text=True)
 (stage/('anchor-negative-'+name+'.log')).write_text(r.stdout+r.stderr)
 expected=0 if name=='fixed-original' else 2
 assert r.returncode==expected,(name,r.returncode,r.stdout,r.stderr)
 if expected:assert 'independent chunk bundle pin mismatch' in r.stderr
 results.append(dict(case=name,exit_code=r.returncode,expected_exit_code=expected,sha256=hashlib.sha256(data).hexdigest(),command=command))
(stage/'ANCHOR_NEGATIVE_TESTS.json').write_text(json.dumps(dict(fixed_bundle_unmodified=hashlib.sha256((stage/'full12-chunks.anchor').read_bytes()).hexdigest()==hashlib.sha256(raw).hexdigest(),tests=results,compile_command=compile_cmd,link_command=link),indent=2))
print('FULL12_ANCHOR_NEGATIVES_PASSED',len(cases),flush=True)
