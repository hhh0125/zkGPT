from pathlib import Path
import subprocess,shlex
report=Path(__file__).resolve().parent;root=report.parent.parent;build=root/'build/release/src/paper_r7'
flags=(build/'CMakeFiles/r7_perf4_u_tests.dir/flags.make').read_text();defs=next(l.split(' = ',1)[1] for l in flags.splitlines() if l.startswith('CXX_DEFINES'));inc=next(l.split(' = ',1)[1] for l in flags.splitlines() if l.startswith('CXX_INCLUDES'));link=shlex.split((build/'CMakeFiles/r7_perf4_u_tests.dir/link.txt').read_text());libs=link[link.index('-o')+2:]
with (report/'FRBUFFER_BUILD.log').open('w') as log:subprocess.run(['/usr/bin/c++',*shlex.split(defs),*shlex.split(inc),'-pthread','-O3','-DNDEBUG','-std=gnu++17',str(report/'frbuffer-lifecycle.cpp'),'-o',str(report/'frbuffer-lifecycle'),*libs],cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
subprocess.run([str(report/'frbuffer-lifecycle')],check=True,stdout=(report/'FRBUFFER_LIFECYCLE.log').open('w'))
