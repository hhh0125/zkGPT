from pathlib import Path,PurePosixPath
import io,tarfile
REMOTE_TOOLS={'tools/oneblock_guard_tests.py','tools/oneblock_regressions.py','tools/oneblock_tests.py','tools/r7_oneblock_runner.py','tools/stage3b1_transport_tests.py','tools/stage3b1_tests.py','tools/stage3a_tests.py','tools/r7_round1_runner.py','tools/r7_round2_runner.py','tools/r7_interactive_tests.py','tools/r7_sync_safety.py','tools/r7_process.py','tools/r7_engineering_tests.py','tools/r7_hyrax_a3_runner.py','tools/hyrax_a3_tests.py','tools/r7_hyrax_a3_stage2_runner.py','tools/hyrax_a3_stage2_tests.py','tools/hyrax_a3_stage2_setup.py'}

def sync_state_target(root):
 return no_links(Path(root),Path(root)/'reports/HYRAX_A3_LAST_SYNC.json')
def no_links(root,target):
 root=Path(root);target=Path(target)
 if root.resolve()!=root:raise ValueError('root must be canonical')
 try:relative=target.relative_to(root)
 except ValueError:raise ValueError('outside root')
 current=root
 for part in relative.parts:
  current=current/part
  if current.is_symlink() or getattr(current,'is_junction',lambda:False)():raise ValueError('symlink/junction path component')
 if not target.resolve().is_relative_to(root):raise ValueError('resolved target outside root')
 return target
def guarded_target(root,name):
 relative=PurePosixPath(name)
 if relative.is_absolute() or '\\' in name or any(p in ('..','.') for p in relative.parts) or relative.as_posix()!=name:raise ValueError('noncanonical sync path')
 if not (name=='src/CMakeLists.txt' or name in REMOTE_TOOLS or name.startswith('src/paper_r7/')):raise ValueError('path outside R7 allowlist')
 return no_links(Path(root),Path(root)/name)
def extract_stage(data,destination):
 destination=Path(destination)
 if not destination.name.startswith(('R7-STAGE3A-RUN-','R7-ROUND1-RUN-','R7-ROUND2-RUN-','R7-HYRAX-A3-STAGE1-RUN-','R7-HYRAX-A3-STAGE2-RUN-')):raise ValueError('unexpected stage name')
 prefix=('reports',destination.name);seen=set();total=0
 with tarfile.open(fileobj=io.BytesIO(data),mode='r:gz') as t:
  pending=[]
  for m in t.getmembers():
   parts=PurePosixPath(m.name).parts
   if PurePosixPath(m.name).is_absolute() or '\\' in m.name or '..' in parts or parts[:2]!=prefix or len(parts)<2:raise ValueError('archive path outside exact stage')
   if not (m.isfile() or m.isdir()) or m.issym() or m.islnk():raise ValueError('archive links or special files forbidden')
   target=destination.joinpath(*parts[2:]);no_links(destination,target)
   if target in seen:raise ValueError('duplicate archive member')
   seen.add(target);total+=m.size
   if m.size>8*1024*1024 or total>64*1024*1024:raise ValueError('public result archive size bound')
   pending.append((m,target))
  for m,target in pending:
   if m.isdir():target.mkdir(parents=True,exist_ok=True)
   else:
    target.parent.mkdir(parents=True,exist_ok=True)
    with t.extractfile(m) as source:target.write_bytes(source.read())

def extract_stage3b1_file(archive,destination):
 destination=Path(destination)
 if not destination.name.startswith(('R7-STAGE3B1-RUN-','R7-ONEBLOCK-RUN-')):raise ValueError('unexpected large-stage name')
 prefix=('reports',destination.name);seen=set();total=0
 with tarfile.open(archive,mode='r:gz') as t:
  pending=[]
  for m in t.getmembers():
   parts=PurePosixPath(m.name).parts
   if PurePosixPath(m.name).is_absolute() or '\\' in m.name or '..' in parts or parts[:2]!=prefix or len(parts)<2:raise ValueError('archive path outside exact stage')
   if not (m.isfile() or m.isdir()) or m.issym() or m.islnk():raise ValueError('archive links or special files forbidden')
   target=destination.joinpath(*parts[2:]);no_links(destination,target)
   if target in seen:raise ValueError('duplicate archive member')
   seen.add(target);total+=m.size
   if m.size>8*1024*1024 or total>(8 if destination.name.startswith('R7-ONEBLOCK-RUN-') else 2)*1024**3:raise ValueError('Stage3B1 public archive budget')
   pending.append((m,target))
  for m,target in pending:
   if m.isdir():target.mkdir(parents=True,exist_ok=True)
   else:
    target.parent.mkdir(parents=True,exist_ok=True)
    with t.extractfile(m) as source,target.open('wb') as output:
     while block:=source.read(1024*1024):output.write(block)
