"""Compile the public-data sidecar with the exact tested target's flags/link libs."""
from pathlib import Path
import hashlib,json,shlex,subprocess
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==ROOT
BASE=ROOT/'reports/R7-SINGLE-COMPACT-20261104-v1';BUILD=ROOT/'build/release/r7-single-compact-20261104-v1'
target=BUILD/'src/paper_r7/CMakeFiles/r7_single_hyrax_tests.dir'
flags={}
for line in (target/'flags.make').read_text().splitlines():
 if line.startswith(('CXX_DEFINES =','CXX_INCLUDES =','CXX_FLAGS =')):
  name,value=line.split('=',1);flags[name.strip()]=shlex.split(value)
link=shlex.split((target/'link.txt').read_text());compile=[link[0],*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(BASE/'compact_measure.cpp'),'-o',str(BASE/'compact_measure.o')]
for i,token in enumerate(link):
 if token.endswith('single_hyrax_tests.cpp.o'):link[i]=str(BASE/'compact_measure.o')
 if i and link[i-1]=='-o':link[i]=str(BASE/'compact_measure')
with (BASE/'measure-build-2.log').open('xb') as log:
 for command in [compile,link]:
  result=subprocess.run(command,cwd=BUILD/'src/paper_r7',stdout=log,stderr=subprocess.STDOUT);assert result.returncode==0,command
record={'compile':compile,'link':link,'binary_sha256':hashlib.sha256((BASE/'compact_measure').read_bytes()).hexdigest(),'source_sha256':hashlib.sha256((BASE/'compact_measure.cpp').read_bytes()).hexdigest()}
(BASE/'measure-build.json').write_text(json.dumps(record,indent=2));print('PUBLIC_DATA_MEASUREMENT_SIDECAR_BUILT')
