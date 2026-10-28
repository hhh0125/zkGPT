from pathlib import Path
import json,hashlib,shutil,re
ROOT=Path(__file__).resolve().parent.parent
OUT=Path(__file__).resolve().parent
def copy(src,rel):
 src=ROOT/src;dst=OUT/'support'/rel
 dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,dst)
 return {'source':str(src.relative_to(ROOT)).replace('\\','/'),'sha256':hashlib.sha256(dst.read_bytes()).hexdigest(),'bytes':dst.stat().st_size}
entries={}
audit=Path('zkGPT-four-question-audit-20261022')
for n in ['01_OUTPUT_CORRECTNESS.md','02_CONSTRAINT_COVERAGE.md','02_CONSTRAINT_COVERAGE_STATIC.json','03_TRUST_BINDING.md','04_PAPER_PROTOCOL_GAP.md','FINAL_FOUR_QUESTIONS.md','TRUST_BYTE_RECONSTRUCTION.json']:
 rel=Path('prior_audit')/n;entries[str(rel)]=copy(audit/'reports'/n,rel)
for n in ['CHECKPOINT_REFERENCE.json','REFERENCE_COMPARISON.json','FRESH_PUBLIC_IDENTITY.json','HF_PINNED_METADATA.json']:
 src=audit/'evidence/reference'/n
 if (ROOT/src).is_file():
  rel=Path('prior_audit/reference')/n;entries[str(rel)]=copy(src,rel)
for folder,names in [
 ('phase_b3_r6_full12_p0ab_D_20261022/delivery',['FINAL_D_RESULT.json','ACTUAL_RESOURCE_SUMMARY.json','ACTUAL_PROTOCOL_PHASES.json','R6-FULL12-P0AB-20261021-v1-verifier.log','R6-FULL12-P0AB-20261021-v1-verifier.time','R6-FULL12-P0AB-20261021-v1-prover.log','R6-FULL12-P0AB-20261021-v1-prover.time','D_AUTHORIZATION_RECORD.json']),
 ('phase_b3_r6_full12_p0ab_20261021/delivery',['CURRENT_GATE_TESTS.log','CURRENT_GATE_TESTS.json','DRAFT_FAIL_CLOSED.json','PLAN_DRIFT_FAIL_CLOSED.json','TOOLCHAIN_AND_FLAGS.json'])]:
 for n in names:
  rel=Path('R6_existing_evidence')/n;entries[str(rel)]=copy(Path(folder)/n,rel)
for folder in ['phase_b3_r6_full12_p0ab_20261021','phase_b3_r6_full12_p0ab_D_20261022','zkGPT-four-question-audit-20261022']:
 for n in ['WINDOWS_ARCHIVE_VERIFICATION.json','AUDIT_ARCHIVE_VERIFICATION.json']:
  if (ROOT/folder/n).is_file():
   rel=Path('prior_archive_receipts')/folder/n;entries[str(rel)]=copy(Path(folder)/n,rel)
request=Path('C:/Users/tanyt/.codex/attachments/fec79f0e-5e8b-4835-abb4-43f0a4f704d3/已粘贴的文本.txt')
dest=OUT/'support/USER_R7_REQUEST.txt';dest.parent.mkdir(exist_ok=True);dest.write_bytes(request.read_bytes())
(OUT/'support/REUSED_EVIDENCE_MANIFEST.json').write_text(json.dumps(entries,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
print('Reused evidence files',len(entries))
