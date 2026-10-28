from pathlib import Path
import json,hashlib,re,shutil,tarfile,datetime
S=Path(__file__).resolve().parent;W=S.parent;P=S/'support';P.mkdir(exist_ok=True)
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
def dump(p,j):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(j,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
b=json.loads((S/'evidence/BASELINE_BEFORE.json').read_text(encoding='utf-8'));src=W/'zkGPT-four-question-audit-20261022/evidence/workspace'
with tarfile.open(W/'zkGPT-R7-stage1-20261022/archives/SOURCE_EVIDENCE.tar.gz','r:gz') as st:
 source_seen={}
 for member in st:
  assert member.isfile() and not member.issym() and not member.islnk()
  source_seen[member.name]=hashlib.sha256(st.extractfile(member).read()).hexdigest()
assert source_seen==b['source_files']
refs=[]
for p in (S/'delivery').glob('*.md'):
 text=p.read_text(encoding='utf-8')
 assert '\ufffd' not in text and '\x00' not in text,p
 for name,a,z in re.findall(r'(?<![\w/])((?:paper_r[12]/)?[A-Za-z0-9_]+\.(?:cpp|hpp|h)):(\d+)(?:[–-](\d+))?',text):
  cand=[n for n in b['source_files'] if n=='src/'+name or n.endswith('/'+name)]
  # Unqualified wire references in the ledger are in its R1 component section;
  # R2 references are explicitly paper_r2/wire.cpp.
  if len(cand)>1 and name in ['wire.cpp','wire.hpp']:cand=['src/paper_r1/'+name]
  assert len(cand)==1,(p.name,name,cand)
  f=src/cand[0];n=len(f.read_text(encoding='utf-8').split('\n'))
  assert 1<=int(a)<=int(z or a)<=n,(name,a,z,n)
  refs.append({'report':p.name,'source':cand[0],'line':int(a),'end':int(z or a),'physical_LF':True})
assert len(refs)>70,len(refs)
j=json.loads((S/'evidence/STATIC_RESOURCE_AND_OPENING_LEDGER.json').read_text(encoding='utf-8'));wire=json.loads((S/'evidence/EXISTING_PROOF_MESSAGE_INVENTORY.json').read_text(encoding='utf-8'))
for name,p in j['plans'].items():
 assert len(p['openings'])==893
 assert p['CAC_all_openings_bytes_only']==sum(32*x['padded']+64*(x['rows']+1) for x in p['openings'])
 assert wire['artifacts'][name]['all_bytes_consumed'] is True
 assert wire['artifacts'][name]['cryptographic_verification_executed'] is False
assert sum(x['assignments_tested'] for x in json.loads((S/'evidence/MALICIOUS_COUNT_EXPERIMENTS.json').read_text(encoding='utf-8'))['exhaustive'])==184252
for folder in ['evidence','logs','literature']:
 dest=P/folder;dest.mkdir(exist_ok=True)
 for p in (S/folder).iterdir():
  if p.is_file():shutil.copyfile(p,dest/p.name)
for p in S.glob('*.py'):
 dest=P/'scripts'/p.name;dest.parent.mkdir(exist_ok=True);shutil.copyfile(p,dest)
base=P/'baseline';base.mkdir(exist_ok=True)
old=W/'zkGPT-R7-stage1-20261022'
for f in ['archives/R7_STAGE1_20261022_v1.tar.gz','archives/SOURCE_EVIDENCE.tar.gz','WINDOWS_ARCHIVE_VERIFICATION.json','support/PARAMETER_DEPENDENCY_RESTORE_MAP.json']:
 shutil.copyfile(old/f,base/Path(f).name)
assert sha(base/'R7_STAGE1_20261022_v1.tar.gz')==json.loads((old/'WINDOWS_ARCHIVE_VERIFICATION.json').read_text(encoding='utf-8'))['archive_sha256']
# Verify reused data/dependency container and restored bytes afresh. No proof run.
pm=json.loads((base/'PARAMETER_DEPENDENCY_RESTORE_MAP.json').read_text(encoding='utf-8'));ap=W/pm['archive'];assert sha(ap)==pm['archive_sha256']
am=json.loads((ap.parent/'ARCHIVE_MANIFEST.json').read_text(encoding='utf-8'));seen={}
with tarfile.open(ap,'r:gz') as t:
 for m in t:
  assert m.isfile() and not m.issym() and not m.islnk(),m.name
  data=t.extractfile(m).read();h=hashlib.sha256(data).hexdigest()
  if m.name=='ARCHIVE_MANIFEST.json':assert json.loads(data)==am;continue
  assert am[m.name]['sha256']==h and am[m.name]['bytes']==len(data),m.name
  seen[m.name]=h
assert set(seen)==set(am)
for n,it in pm['parameters'].items():assert sha(W/it['restored_cas'])==it['sha256'],n
for n,it in pm['dependencies'].items():
 cas=W/'phase_b3_r6_full12_p0ab_20261021/restore-cas'/it['sha256'];assert sha(cas)==it['sha256'],n
dump(P/'STAGE_RESTORE_MAP.json',{'source_and_R6_evidence_container':'baseline/R7_STAGE1_20261022_v1.tar.gz','source_container':'baseline/SOURCE_EVIDENCE.tar.gz','source_files':575,'source_root_sha256':b['source_root_sha256'],'frozen_R6_proof_sha256':'d088e55ace3c31f510afbb589022d6f2b8dedfd6725444de37be6f70b18762bd','frozen_R6_statement_sha256':'6a73f7a6d71a0c8b4f9d19a08ef4cbf68e4730491d4bf5377b0ae170c7e52958','parameter_dependency_restore_map':'baseline/PARAMETER_DEPENDENCY_RESTORE_MAP.json','reuse_container':pm['archive'],'reuse_container_sha256':pm['archive_sha256'],'reused_container_all_members_freshly_verified':len(seen),'parameter_CAS_restore_reads':len(pm['parameters']),'dependency_CAS_restore_reads':len(pm['dependencies']),'raw_verification_negative_audit_resource_logs':'In the included prior Stage1 evidence container and its explicitly mapped R6 preparation archive; reused unchanged, no new crypto verification','no_deletion_authorized':True})
dump(P/'DOCUMENT_CHECK.json',{'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_files_matched':575,'source_line_citations_checked':len(refs),'references':refs,'required_deliverables':sorted(p.name for p in (S/'delivery').glob('*.md')),'CAC_all_893_opening_sizes_recomputed':True,'both_existing_proof_grammar_complete':True,'source_code_changed':False,'proofs_generated':False,'cryptographic_verification_run':False,'reused_archive_members':len(seen),'parameter_restore_reads':120,'dependency_restore_reads':len(pm['dependencies'])})
(P/'RESTORE.md').write_text('''# Task0-A 研究交付恢复\n\n先核对容器和所有成员 SHA，再读取 delivery/ 四份报告。support/evidence 提供原始 JSON，support/scripts 是研究/只读解析脚本，support/literature 是本次使用的 PDF 字节和页标记文本。仅在新空目录恢复归档；不要覆盖现有工作树或运行其中证明程序。\n\n源码完整字节位于 baseline/SOURCE_EVIDENCE.tar.gz；完整既有 Stage1/R6 evidence、proof/statement 和身份位于 baseline/R7_STAGE1_20261022_v1.tar.gz。参数/依赖通过 STAGE_RESTORE_MAP.json 和 baseline/PARAMETER_DEPENDENCY_RESTORE_MAP.json 逐项映射到已校验 Windows 存储。旧 archive 的绝对路径是出处，不要求已清理的远程 R3/R4/R5A 目录存在。\n\n本次没有生产源码 patch，因为生产字节未变。代数枚举日志是研究证据；既有 verifier/negative-test 日志是历史复用，不能标注为本轮重新验证。未归档 SSH 配置、私钥或 pdf_tools 安装目录。所有远端副本继续保留，不授权删除。\n''',encoding='utf-8')
print(json.dumps({'source_files':575,'source_line_citations':len(refs),'reused_archive_members':len(seen),'parameter_restore_reads':120,'dependency_restore_reads':len(pm['dependencies']),'reports':4},indent=2))
