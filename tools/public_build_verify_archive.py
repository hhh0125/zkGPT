from pathlib import Path
import hashlib,json,tarfile,sys
archive=Path(sys.argv[1]);receipt=json.loads(Path(sys.argv[2]).read_text());out=archive.parent/'restore-cas';out.mkdir(exist_ok=True)
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
assert sha(archive)==receipt['sha256'];restore={}
with tarfile.open(archive,'r:gz') as t:
 manifest_bytes=t.extractfile('ARCHIVE_MANIFEST.json').read();assert hashlib.sha256(manifest_bytes).hexdigest()==receipt['manifest_sha256'];manifest=json.loads(manifest_bytes);assert len(manifest)==receipt['files'];members=[m for m in t.getmembers() if m.name!='ARCHIVE_MANIFEST.json'];assert len(members)==len(manifest)
 for m in members:
  assert m.isfile() and m.name in manifest and '..' not in Path(m.name).parts and not m.name.startswith('/'),m.name
  expected=manifest[m.name];target=out/expected['sha256'];h=hashlib.sha256();size=0
  with t.extractfile(m) as src,target.open('wb') as dest:
   for data in iter(lambda:src.read(4<<20),b''):h.update(data);size+=len(data);dest.write(data)
  assert h.hexdigest()==expected['sha256'] and size==expected['bytes'] and sha(target)==expected['sha256'],m.name
  restore[m.name]=expected|{'cas':target.name,'mode':m.mode}
 # Consume through the gzip trailer, checking CRC rather than trusting member EOF.
import gzip
with gzip.open(archive,'rb') as f:
 while f.read(4<<20):pass
Path(str(archive)+'.restore-map.json').write_text(json.dumps(restore,indent=2))
result={'success':True,'archive_sha256':receipt['sha256'],'regular_files':len(restore),'every_member_sha_verified':True,'restored_bytes_reread':True,'gzip_crc':True};Path(str(archive)+'.LOCAL_VERIFICATION.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
