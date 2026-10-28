from pathlib import Path
import hashlib,json,tarfile,datetime,re
ROOT=Path(__file__).resolve().parent.parent
OUT=Path(__file__).resolve().parent
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
def write(p,j):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(j,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
base=ROOT/'phase_b3_r6_full12_p0ab_20261021'
am=json.loads((base/'archives/ARCHIVE_MANIFEST.json').read_text())
ap=base/'archives/R6_FULL12_P0AB_PREPARATION_v1.tar.gz'
receipt=json.loads((base/'archives/ARCHIVE_RECEIPT.json').read_text())
print('Receipt keys',list(receipt))
container=sha(ap)
assert container in json.dumps(receipt), 'prior archive container receipt mismatch'
checked={}
with tarfile.open(ap,'r:gz') as t:
 for m in t:
  assert m.isfile() and not m.issym() and not m.islnk(),m.name
  b=t.extractfile(m).read();h=hashlib.sha256(b).hexdigest()
  if m.name=='ARCHIVE_MANIFEST.json':
   assert json.loads(b)==am and h==receipt['manifest_sha256'];continue
  assert am[m.name]['sha256']==h and am[m.name]['bytes']==len(b),m.name
  checked[m.name]=h
assert set(checked)==set(am)
supp=json.loads((OUT/'evidence/BASELINE_SUPPLEMENT.json').read_text())
parammap={}
for name,item in supp['protected'].items():
 if name.startswith('data/gpt2_int/'):
  member='workspace/'+name;assert checked[member]==item['sha256']
  restored=base/'restore-cas'/item['sha256'];assert restored.is_file() and sha(restored)==item['sha256']
  parammap[name]={'archive_member':member,'sha256':item['sha256'],'bytes':item['bytes'],'restored_cas':str(restored.relative_to(ROOT)).replace('\\','/')}
assert len(parammap)==120
deps={k:v for k,v in am.items() if k.startswith('runtime/') or k.startswith('workspace/deps/') or k.endswith('libmcl.a')}
for name,item in deps.items():
 restored=base/'restore-cas'/item['sha256'];assert restored.is_file() and sha(restored)==item['sha256'],name
write(OUT/'support/PARAMETER_DEPENDENCY_RESTORE_MAP.json',{'archive':str(ap.relative_to(ROOT)).replace('\\','/'),'archive_sha256':container,'all_archive_members_freshly_verified':len(checked),'gzip_and_membership_complete':True,'parameters':parammap,'dependencies':deps,'restore_read':True,'notes':'Unchanged R6 data/dependencies reused by explicit per-stage map. No credentials or keys.'})
fullplan=ROOT/'zkGPT-four-question-audit-20261022/evidence/workspace/reports/R6-FULL12-P0AB-20261021/FULL12_PLAN.json'
oneplan=OUT/'support/R6_existing_evidence/R6_ONEBLOCK_PLAN.json'
descriptors={}
for tag,path,blocks in [('full12',fullplan,12),('test_oneblock',oneplan,1)]:
 j=json.loads(path.read_text()); assert j['blocks']==blocks
 d={'arithmetic_spec':'R7-frozen-R6-integer-v1','purpose':'formal-full12' if blocks==12 else 'test-oneblock-only','source_sha256':'5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416','field_modulus':'16798108731015832284940804142231733909759579603404752749028378864165570215949','shape':[32,blocks,12,64,768,3072],'profile_sha256':j['profile_sha256'],'exp_table_sha256':'062c686f919b356c9f8c47acada933bc2683c13ac8886a0cb881239e807f21dc','circuit_fingerprint':j['circuit'],'frozen_plan_sha256':sha(path),'master_actual':j['master_actual'],'master_padded':j['master_padded'],'W_actual':j['W_actual'],'W_padded':j['W_padded'],'layers_including_master':j['layers'],'terminals':j['terminals'],'range_values':j['range_values'],'exp_actual':j['Exp_actual'],'input_encoding':'signed64LE-padded32x1024','output_semantics':f'post-second-residual-hidden-block{blocks}','auxiliary_semantics':'preserve-R6-existential-LN-padding-freedom','anchor_policy':'independently-registered-hiding-full12' if blocks==12 else 'independently-registered-hiding-test-first-block-subset'}
 canonical=json.dumps(d,sort_keys=True,separators=(',',':'),ensure_ascii=True).encode('ascii')
 descriptors[tag]={'descriptor':d,'relation_id':hashlib.sha256(b'zkGPT/R7/integer-relation/v1\0'+canonical).hexdigest()}
write(OUT/'delivery/R7_RELATION_DESCRIPTORS.json',{'schema':'R7-INTEGER-RELATION-DESCRIPTORS-v1','canonicalization':'ASCII json.dumps(descriptor,sort_keys=True,separators=(comma,colon),ensure_ascii=True); no floats; SHA256(prefix||canonical), prefix ASCII zkGPT/R7/integer-relation/v1 followed by NUL','relations':descriptors})
gatepath=OUT/'delivery/R7_STAGE1_GATE.json';gate=json.loads(gatepath.read_text())
gate['design_review']='independent builder review completed; oneblock relation/anchor issue resolved in specification; not a cryptographic security proof'
gate['relation_descriptors']={k:v['relation_id'] for k,v in descriptors.items()}
gate['verification_receipts']=['BASELINE_BEFORE.json','BASELINE_SUPPLEMENT.json','PRESERVATION_AFTER.json','../WINDOWS_ARCHIVE_VERIFICATION.json (external receipt after final tar download)']
write(gatepath,gate)
source=json.loads((OUT/'evidence/BASELINE_BEFORE.json').read_text())['source_files']
byname={k:ROOT/'zkGPT-four-question-audit-20261022/evidence/workspace'/k for k in source}
refs=[]
for p in (OUT/'delivery').glob('*.md'):
 text=p.read_text(encoding='utf-8')
 for name,start,end in re.findall(r'(?<![\w/])((?:paper_r[12]/)?[A-Za-z0-9_]+\.(?:cpp|hpp|h)):(\d+)(?:[–-](\d+))?',text):
  rel='src/'+name
  if rel not in source:
   candidates=[k for k in source if k.endswith('/'+name)]
   assert len(candidates)==1,(p.name,rel,candidates)
   rel=candidates[0]
  f=byname[rel]; assert f.is_file() and sha(f)==source[rel],rel
  n=len(f.read_text(encoding='utf-8').splitlines());assert 1<=int(start)<=int(end or start)<=n,(rel,start,end,n)
  refs.append({'document':p.name,'source':rel,'start':int(start),'end':int(end or start)})
assert len(refs)>40
assert gate['may_enter_stage2'] is False and gate['r7_zero_knowledge_implemented'] is False
write(OUT/'support/DOCUMENT_CHECK.json',{'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_citations_with_line_numbers_checked':len(refs),'references':refs,'all_six_required_files_present':all((OUT/'delivery'/n).is_file() for n in gate['deliverables']),'full12_and_oneblock_descriptors_match_existing_plans':True,'gate_fail_closed':True,'prior_archive_members_verified':len(checked),'parameter_restore_reads':len(parammap),'dependency_restore_reads':len(deps),'scope':'documentation/identity checks; not R7 security or proof execution'})
print('Archive members',len(checked),'parameters',len(parammap),'deps',len(deps),'source citations',len(refs));print('Relation IDs',{k:v['relation_id'] for k,v in descriptors.items()})
