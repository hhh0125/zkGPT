from pathlib import Path,PurePosixPath
import subprocess,hashlib,json,tarfile,gzip,re,datetime,shutil,sys
root=Path(__file__).resolve().parent;arcdir=root/'archives';arcdir.mkdir(exist_ok=False)
assert shutil.disk_usage(root).free>2<<30
remote='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-OPT2-20261021/'
for name in ['R6B_OPT2_final_evidence_v1.tar.gz','R6B_OPT2_final_evidence_v1-receipt.json']:
 subprocess.run(['scp','-F','4090_sub2api_ssh_config','4090_sub2api:'+remote+name,str(arcdir/name)],check=True)
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
receipt=json.loads((arcdir/'R6B_OPT2_final_evidence_v1-receipt.json').read_text(encoding='utf-8'));archive=arcdir/receipt['archive'];assert sha(archive)==receipt['sha256']==sys.argv[1];assert archive.stat().st_size==receipt['bytes']
cas=root/'restore-cas';cas.mkdir(exist_ok=False);delivery=root/'delivery';delivery.mkdir(exist_ok=False);seen=set()
with tarfile.open(archive) as t:
 for m in t:
  n=PurePosixPath(m.name);assert m.isfile() and n.parts[:2] in [('reports','R6B-OPT2-20261021'),('logs','R6B-OPT2-20261021')] and not n.is_absolute() and '..' not in n.parts and ':' not in m.name and '\\' not in m.name and m.name not in seen
  row=receipt['files'][m.name];b=t.extractfile(m).read();assert re.fullmatch('[0-9a-f]{64}',row['sha256']);assert len(b)==row['bytes']==m.size and hashlib.sha256(b).hexdigest()==row['sha256']
  p=cas/row['sha256']
  if not p.exists():p.write_bytes(b)
  assert sha(p)==row['sha256'];seen.add(m.name)
  if len(n.parts)==3:
   p=delivery/n.name
   if p.exists():assert sha(p)==row['sha256']
   else:p.write_bytes(b)
assert seen==set(receipt['files']) and len(seen)==receipt['file_count']
with gzip.open(archive,'rb') as f:
 while f.read(1048576):pass
stage='reports/R6B-OPT2-20261021/';status=json.loads((delivery/'FINAL_STATUS.json').read_text(encoding='utf-8'));identity=json.loads((delivery/'R6B_IDENTITY_CHECK.json').read_text(encoding='utf-8'));gate=json.loads((delivery/'R6B_EXECUTION_GATE_SOURCE_SHA.json').read_text(encoding='utf-8'));changes=json.loads((delivery/'SOURCE_CHANGE_MANIFEST.json').read_text(encoding='utf-8'))
assert status['success'] and status['Release']=='69/69' and status['targeted_ASan_UBSan']=='4/4' and status['oneblock_independent_verifier']=='ACCEPT' and status['gate_tests']=='23/23' and status['gate_successful_subcases']==50
assert not any(status[k] for k in ['execution_authorized','valid_permit_generated','permit_SHA_approved_by_user','full12_witness_executed','full12_proof_generated','full12_independent_verifier_executed','formal_A_B_C_D_workflow_called'])
assert len(changes)==16
for n,row in changes.items():assert receipt['files'][stage+'after/'+n]['sha256']==row['after_sha256']
for mode,row in identity['selected_executables'].items():assert receipt['files'][stage+'optimized-binaries/r6_model_'+('prove' if mode=='prover' else 'verify')]['sha256']==row['sha256']
for n,row in gate['production_files'].items():assert sha(delivery/n)==row['sha256']
canonical=lambda obj:json.dumps(obj,sort_keys=True,separators=(',',':')).encode()
assert hashlib.sha256(canonical({k:gate[k] for k in ['production_files','runtime_library_files','reused_protection']})).hexdigest()==gate['gate_identity_sha256']==status['gate_identity_sha256']
for n in ['model.statement','model.proof']:
 a=receipt['files'][stage+'optimized-oneblock/'+n];b=receipt['files'][stage+'final-oneblock/'+n];assert a['sha256']==b['sha256'] and a['bytes']==b['bytes']
for folder,h in [('phase_b3_r6a_20261020','57498e9926f81111bc9e1a476584fab18df379bdf383d37a8b50dc9da57f9828'),('phase_b3_r6b_gate_20261020','4e56e365a1b40ac13d291c023c93b6a05786d5ad2b82e99fff4deeb887fcef5f'),('phase_b3_r6b_opt_20261021','fc6c068adf0a51f136cc460fee946e33f6787debfa6388649116873b505794a4')]:
 prior=json.loads((root.parent/folder/'LOCAL_RESTORE_RECEIPT.json').read_text(encoding='utf-8'));assert sha(prior['archive'])==prior['archive_sha256']==h
param=json.loads((root.parent/'phase_b3_r6a_20261020/delivery/PARAMETER_RESTORE_MAP.json').read_text(encoding='utf-8'));assert len(param)==120
for n,row in param.items():assert sha(row['windows_cas'])==row['sha256']
result={'success':True,'verified_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'remote_stage':remote.rstrip('/'),'archive':str(archive),'archive_sha256':receipt['sha256'],'archive_bytes':receipt['bytes'],'actual_restored_payloads':len(seen),'all_members_size_SHA_actual_restore_and_reread':True,'gzip_complete_stream_CRC':True,'all16changed_source_bytes_checked':True,'new_prover_verifier_parser_gate_SHA_checked':True,'oneblock_proof_statement_match':True,'prior_R6A_R6B_OPT_archives_rehashed':True,'all120parameter_CAS_rehashed':True,'source_tree_sha256':status['source_tree_sha256'],'gate_identity_sha256':status['gate_identity_sha256'],'valid_permits_generated':False,'formal_authorization_workflow_called':False,'full12_witness_proof_or_independent_verifier_executed':False,'remote_disposal_authorized':False}
for p in [root/'LOCAL_RESTORE_RECEIPT.json',delivery/'WINDOWS_RESTORE_RECEIPT.json']:assert not p.exists();p.write_text(json.dumps(result,indent=2),encoding='utf-8')
(delivery/'交付结果.md').write_text('''# R6B-OPT2 交付

实际优化及最终oneblock独立ACCEPT完成；full12未运行，无有效permit，未进入正式授权流程。

- [七项验收回答与前后对比](R6B_OPT2_MEMORY_REPORT.md)
- [分阶段精确核算](FULL12_RESOURCE_PLAN_OPT2.json)、[真实地址/所有权](OBJECT_ADDRESS_LIFETIME_EVIDENCE.json)
- [实测RSS/CPU/I-O及分配](MEASURED_MEMORY_CPU_IO.json)
- [源码变更清单](SOURCE_CHANGE_MANIFEST.json)、[补丁](R6B_OPT2_SOURCE.patch)
- [最终身份](R6B_IDENTITY_CHECK.json)、[旧身份失效](SUPERSEDED_IDENTITIES.json)
- [最终proof字节对照](FINAL_ONEBLOCK_BYTE_COMPATIBILITY.json)、[协议兼容理由](PROTOCOL_COMPATIBILITY.md)
- [回归及最终状态](FINAL_STATUS.json)、[资源快照](FINAL_RESOURCE_SNAPSHOT.json)
- [恢复说明](RESTORE_OPT2.md)、[Windows实际恢复收据](WINDOWS_RESTORE_RECEIPT.json)

RSS4.889→4.390GiB；full12预算109.223→84.450GiB。确认重复10GiB（含25%）；真实共享Master预算效应10GiB；val0容量包络收紧4.773GiB不能冒充已测full12收益。额外两份graph和其他安全余量均保留，建议下一轮单独评估P96/V96。归档全部成员实际恢复和SHA重读通过；旧R6A/R6B/OPT容器及120参数CAS重哈希通过。无删除或正式运行授权。
''',encoding='utf-8')
print(json.dumps(result,indent=2))
