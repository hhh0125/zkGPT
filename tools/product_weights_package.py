"""Archive this completed stage only; never remove source or evidence."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import tarfile
import sys

ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
STAGE=ROOT/'reports/R7-PRODUCT-WEIGHT/R7-PRODUCT-WEIGHT-OPT-20261112-v1'
BUILD=ROOT/'build/product-weight-20261112-v1'
ARCHIVE=STAGE.with_suffix('.tar.gz')

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda:f.read(4<<20),b''):h.update(chunk)
    return h.hexdigest()

def save(name,data):
    path=STAGE/name;assert not path.exists(),path
    path.write_text(json.dumps(data,indent=2)+'\n')

def main():
    assert json.loads((STAGE/'EXPERIMENTS_RESULT.json').read_text())['success']
    assert not ARCHIVE.exists(),'refuse archive overwrite'
    # Preserve the exact runner used by the long experiment before any helper
    # maintenance. The production source identity was frozen separately.
    executed=STAGE/'EXECUTED_RUNNER.py'
    if not executed.exists():shutil.copy2(ROOT/'tools/product_weights_remote.py',executed)
    shutil.copy2(ROOT/'tools/product_weights_package.py',STAGE/'PACKAGING_SCRIPT.py')
    candidate=STAGE/'candidate-bin';candidate.mkdir(exist_ok=True)
    for name in ['r7_fs_full12_prover','r7_fs_full12_verifier','r7_perf6_exact_native','r7_perf5_product_fs_probe','r7_product_weight_tests']:
        shutil.copy2(BUILD/'src/paper_r7'/name,candidate/name)
    config=STAGE/'build-identity';config.mkdir()
    shutil.copy2(BUILD/'CMakeCache.txt',config/'CMakeCache.txt')
    for name in ['flags.make','link.txt']:
        for target in ['paper_r7','r7_fs_full12_prover','r7_fs_full12_verifier']:
            path=BUILD/'src/paper_r7/CMakeFiles'/(target+'.dir')/name
            if path.exists():shutil.copy2(path,config/(target+'-'+name))
    tools={name:subprocess.check_output(args,text=True).strip() for name,args in {'uname':['uname','-a'],'compiler':['c++','--version'],'cmake':['cmake','--version'],'openssl':['openssl','version']}.items()}
    dependencies={}
    for name in ['r7_fs_full12_prover','r7_fs_full12_verifier']:
        text=subprocess.check_output(['ldd',str(candidate/name)],text=True)
        libraries={}
        for line in text.splitlines():
            for word in line.split():
                if word.startswith('/') and Path(word).is_file():libraries[word]=sha(word)
        dependencies[name]={'ldd':text,'library_sha256':libraries,'binary_sha256':sha(candidate/name)}
    save('TOOLCHAIN_AND_DEPENDENCIES.json',{'tools':tools,'dependencies':dependencies})
    method=json.loads((STAGE/'METHOD.json').read_text())
    method['full12_environment']={k:v for k,v in method['full12_environment'].items() if k.startswith('ZKGPT_') or k=='MALLOC_ARENA_MAX'}
    save('METHOD_PUBLIC.json',method)
    files={}
    def add_tree(root,prefix):
        for p in root.rglob('*'):
            if p.is_file() and not p.is_symlink():files[prefix+'/'+str(p.relative_to(root))]=p
    add_tree(ROOT/'src','source/src')
    files['source/CMakeLists.txt']=ROOT/'CMakeLists.txt'
    files['source/build.sh']=ROOT/'build.sh'
    files['source/AGENTS.md']=ROOT/'AGENTS.md'
    mcl=ROOT/'3rd/mcl'
    for p in mcl.rglob('*'):
        rel=p.relative_to(mcl)
        if p.is_file() and not p.is_symlink() and not any(part in ['.git','obj','lib','bin','build','__pycache__'] for part in rel.parts) and p.suffix.lower() not in ['.obj','.o','.pdb','.pyc']:
            files['source/3rd/mcl/'+str(rel)]=p
    add_tree(ROOT/'deps/cnpy','dependency/cnpy')
    add_tree(STAGE,'evidence')
    files.pop('evidence/METHOD.json',None) # only scoped task environment is public
    add_tree(ROOT/'logs'/STAGE.name,'logs')
    add_tree(ROOT.parent/'zkGPT-work-backup'/STAGE.name,'baseline-overwrite')
    r9=ROOT/'reports/R9-FULL12-PHASE-TIMING/R9-FULL12-PHASE-TIMING-20261111-v1'
    for name in ['P-SCOPES.tsv','V-SCOPES.tsv','PARAMETERS.json','run_experiments.py','FROZEN_SOURCE.json','FINAL_REPORT.zh-CN.md']:
        files['reference/R9/'+name]=r9/name
    manifest={name:{'sha256':sha(path),'bytes':path.stat().st_size} for name,path in sorted(files.items())}
    save('ARCHIVE_MANIFEST.json',manifest)
    files['evidence/ARCHIVE_MANIFEST.json']=STAGE/'ARCHIVE_MANIFEST.json'
    with tarfile.open(ARCHIVE,'w:gz',compresslevel=3) as tar:
        for name,path in sorted(files.items()):tar.add(path,arcname=name,recursive=False)
    save('ARCHIVE_RECEIPT.json',{'path':str(ARCHIVE),'bytes':ARCHIVE.stat().st_size,'sha256':sha(ARCHIVE),'manifest_sha256':sha(STAGE/'ARCHIVE_MANIFEST.json'),'files':len(files),'remote_copies_disposable':False})
    print((STAGE/'ARCHIVE_RECEIPT.json').read_text())

if __name__=='__main__':
    if len(sys.argv)>1 and sys.argv[1]=='capture':
        target=STAGE/'EXECUTED_RUNNER.py';assert not target.exists()
        shutil.copy2(ROOT/'tools/product_weights_remote.py',target)
        print('Executed runner SHA256',sha(target))
    else:main()
