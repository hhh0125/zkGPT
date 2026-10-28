"""Small fail-closed utilities shared by the R6B gate."""
from pathlib import Path
import os,stat,hashlib,json,struct
X_SHA='a1871e2f9731ed73e1b5db9655ee760d3e5435a06d793c09f1dc1eb5f6655edb'
class GateError(RuntimeError):pass
def require(ok,message):
    if not ok:raise GateError(message)
def canonical(value):return json.dumps(value,sort_keys=True,separators=(',',':')).encode()
def digest(value):return hashlib.sha256(value).hexdigest()
def path_under(path,root):
    path=Path(os.path.abspath(path));root=Path(root).resolve()
    require(path.is_relative_to(root) and path!=root,'path outside allowed root')
    for at in [path,*path.parents]:
        if at==root:break
        require(not at.is_symlink(),'symlink path forbidden: '+str(at))
    return path
def read_regular(path,cap=64<<20):
    path=Path(path);require(not path.is_symlink(),'symlink file forbidden')
    fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW)
    try:
        before=os.fstat(fd);require(stat.S_ISREG(before.st_mode) and before.st_size<=cap,'file type/size cap')
        with os.fdopen(fd,'rb',closefd=False) as f:b=f.read(cap+1)
        after=os.fstat(fd);require(len(b)<=cap and (before.st_ino,before.st_size,before.st_mtime_ns,before.st_ctime_ns)==(after.st_ino,after.st_size,after.st_mtime_ns,after.st_ctime_ns),'file changed while reading')
        return b
    finally:os.close(fd)
def file_sha(path,cap=256<<20):return digest(read_regular(path,cap))
def parse_json_bytes(data):
    def pairs(items):
        d={}
        for k,v in items:require(k not in d,'duplicate JSON key');d[k]=v
        return d
    return json.loads(data,object_pairs_hook=pairs)
def json_read(path):return parse_json_bytes(read_regular(path,16<<20))
def pinned_json(path,pin):
    data=read_regular(path,16<<20);require(digest(data)==pin,'independent approval SHA mismatch');return parse_json_bytes(data)
def write_new(path,value):
    b=value if isinstance(value,bytes) else json.dumps(value,sort_keys=True,indent=2).encode()
    with Path(path).open('xb') as f:f.write(b);f.flush();os.fsync(f.fileno())
def validate_public_x(values):
    require(len(values)==32768,'X must contain32x1024 values')
    for i,v in enumerate(values):
        require(type(v) is int and -(1<<63)<=v<(1<<63),'X signed64 domain')
        require(i%1024<768 or v==0,'X padding must be zero')
    h=digest(b''.join(struct.pack('<q',v) for v in values));require(h==X_SHA,'X complete frozen SHA mismatch');return h
def child_environment(ambient=None):
    return {'PATH':'/usr/bin:/bin','LANG':'C.UTF-8','LC_ALL':'C','TZ':'UTC','ZKGPT_THREADS':'4','OMP_NUM_THREADS':'4','OPENBLAS_NUM_THREADS':'4','MALLOC_ARENA_MAX':'2'}
