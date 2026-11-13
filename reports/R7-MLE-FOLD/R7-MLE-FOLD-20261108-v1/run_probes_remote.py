from pathlib import Path
import subprocess,shlex,os,json
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(R);S=R/'reports/R7-MLE-FOLD-20261108-v1';B=R/'build/release/r7-mle-fold-20261108-v1';old=R/'reports/R7-CATEGORY-X-20261106-v1';unit=B/'src/paper_r7/CMakeFiles/r7_category_mle_fold_tests.dir';flags={}
for line in (unit/'flags.make').read_text().splitlines():
 if ' = ' in line:k,v=line.split(' = ',1);flags[k]=v
results=[]
for name in ['category_kernel_clipping_probe','category_x_binding_probe','callback_lifetime_regression']:
 source=S/(name+'.cpp')
 if name!='callback_lifetime_regression':source.write_bytes((old/(name+'.cpp')).read_bytes())
 obj=S/(name+'.o');args=['c++']+sum((shlex.split(flags.get(k,'')) for k in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']),[])+['-c',str(source),'-o',str(obj)]
 p=subprocess.run(args,capture_output=True,text=True);assert p.returncode==0,p.stderr
 link=shlex.split((unit/'link.txt').read_text());at=next(i for i,v in enumerate(link) if v.endswith('category_mle_fold_tests.cpp.o'));link[at]=str(obj);exe=S/name;link[link.index('-o')+1]=str(exe)
 p=subprocess.run(link,cwd=B/'src/paper_r7',capture_output=True,text=True);assert p.returncode==0,p.stderr
 p=subprocess.run([exe],capture_output=True,text=True);(S/(name+'.log')).write_text(p.stdout+p.stderr);assert p.returncode==0,(name,p.stderr);results.append({'name':name,'status':'PASS','log':name+'.log'})
(S/'PROBES_RESULT.json').write_text(json.dumps(results,indent=2));print(json.dumps(results))
