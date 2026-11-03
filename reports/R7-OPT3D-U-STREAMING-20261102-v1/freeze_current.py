from control import *
current={f:sha(REPO/f) for f in files()};dump('CURRENT_SOURCE_SHA.json',current)
result=remote('import pathlib,json,hashlib\np=pathlib.Path('+repr(REMOTE)+')\ne='+repr(current)+'\nassert all(hashlib.sha256((p/f).read_bytes()).hexdigest()==h for f,h in e.items())\n(p/'+repr(STAGE)+'/"CURRENT_SOURCE_SHA.json").write_text(json.dumps(e,indent=2))\nprint("SOURCE_SHA_MATCH",len(e))')
print(result.decode())
for f in current:
 dst=ROOT/'after'/f;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes((REPO/f).read_bytes());assert sha(dst)==current[f]
