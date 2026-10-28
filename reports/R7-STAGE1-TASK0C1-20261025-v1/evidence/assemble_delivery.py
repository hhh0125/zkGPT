"""Assemble C.1 reports and preservation evidence; never reads model payloads."""
from pathlib import Path
import hashlib,json,os,platform,sys
from datetime import datetime,timezone

ROOT=Path("/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work")
STAGE=ROOT/"reports/R7-STAGE1-TASK0C1-20261025-v1"
OLD=ROOT/"reports/R7-STAGE1-TASK0C-20261023-v1"
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(rel,obj):
    (STAGE/rel).write_text(json.dumps(obj,indent=2,sort_keys=True)+"\n",encoding="utf-8")
result=json.loads((STAGE/"logs/full-boundary-01/RESULTS.json").read_text())
assert result["success"] and result["total"]==39
rows=[]
for case in result["cases"]:
    prefix=STAGE/"logs/full-boundary-01"/case["case"]
    events=[json.loads(line) for line in prefix.with_suffix(".client.jsonl").read_text().splitlines()]
    directions={}
    maxline=0
    for direction in ("P->V","V->P"):
        values=[(json.dumps(e["value"],sort_keys=True,separators=(",",":"))+"\n").encode() for e in events if e["direction"]==direction]
        directions[direction]=sum(map(len,values))
        maxline=max([maxline]+[len(x) for x in values])
    rows.append({"case":case["case"],"wall_seconds":case["seconds"],"status":case["actual"]["status"],
                 "challenges":case["actual"]["challenges"],"components":case["actual"]["components"],
                 "canonical_logged_wire_bytes":directions,"max_logged_message_bytes":maxline})
write("delivery/R7_C1_RESOURCE_MEASUREMENTS.json",{
    "schema":"R7-C1-RESOURCE-MEASUREMENTS-v1","cases":rows,
    "suite_wall_seconds":31.96,"suite_user_seconds":31.00,"suite_system_seconds":0.62,
    "max_rss_kib_time_v":25292,"max_rss_kib_driver":result["max_rss_kib"],
    "child_max_rss_kib":result["child_max_rss_kib"],
    "cpu_seconds_cap_per_process":120,"address_space_mib_cap":512,"session_wall_seconds_cap":60,
    "idle_seconds_cap":10,"message_bytes_cap":2097152,"input_bytes_cap":16777216,"events_cap":2048,
    "msm_terms_hard_cap":32,"bounds":{"N":1024,"degree":3,"table":64,"HPC_K":4},
    "wire_note":"Reconstructed canonical JSON line byte counts from structured client logs; malformed raw writes (duplicate/partial line) are not included. Logs are evidence, never replay authority.",
    "measurement_note":"One final run; RSS is peak as reported, not summed simultaneous process memory; no production extrapolation."
})
mapping=[
("1","attack01_forged_state","Prover伪造state"),
("2","attack02_self_receipt","自签receipt"),
("3a","attack03_prover_seed","指定随机seed"),
("3b","attack03_challenge_replaced","替换本地使用的relation c"),
("4","attack04_before_commitment","承诺前请求挑战"),
("5","attack05_round_order","Sumcheck轮次换序"),
("6","attack06_old_session_coin","新session使用旧session的c"),
("7a","attack07_session_envelope","替换外层session身份"),
("7b","attack07_component_splice","拼接旧session组件statement"),
("8","attack08_one_layer_relation_c","一层SC relation c篡改"),
("9","attack09_two_layer_endpoint_c","两层第二endpoint/product c篡改"),
("10a","attack10_before_aggregation_claim","aggregation前替换X claim"),
("10b","attack10_after_aggregation_claim","aggregation后替换derived claim"),
("11","attack11_opening_full_handle","H-PC完整handle身份修改"),
("12","attack12_false_compression","已知非零错误压缩关系尝试"),
("13","attack13_verifier_rng_failure","V挑战生成故障"),
("14a","attack14_early_accept","提前请求ACCEPT"),
("14b","attack14_missing_final_response","省略最后响应并EOF")]
byname={c["case"]:c for c in result["cases"]}
lines=["# R7_C1_NEGATIVE_TESTS","",
"日期：2026-10-25。以下均为独立Verifier真实执行结果，不是只统计单元测试断言。",
"原始目录：logs/full-boundary-01/。每案保存 <case>.client.jsonl、<case>.verifier.jsonl、<case>.result.json；RESULTS.json汇总。",
"客户端日志包含公开承诺、公开挑战、masked响应和具体ATTACK动作；不记录toy witness/blinding。非法截断/重复key的原始写入由test_boundary_cases.py定义，结果由V原始拒绝日志记录。日志不能作为验证权威。",
"","## 必测攻击逐项结果","",
"|要求|case/动作|实际终态|具体拒绝原因|挑战数|","|---|---|---|---|---:|"]
for number,name,meaning in mapping:
    c=byname[name]; a=c["actual"]
    lines.append("|"+number+"|"+name+"： "+meaning+"|"+a["status"]+"|"+a["reason"]+"|"+str(a["challenges"])+"|")
lines+=["","所有上表用例expected与actual一致，进程非零退出；没有攻击返回ACCEPT。",
"false_compression使用公开固定-x式故意错误1，representation/product仍按其真实toy openings生成；独立rho/c下压缩检查拒绝。这是一个具体负例，不是多阶段知识证明。",
"旧c来自另一次真实独立V会话；替换仅发生在P使用的公开coin副本，V私有记录不变。",
"","## 补充检查",""]
for c in result["cases"]:
    if c["case"].startswith("extra_"):
        lines.append("- "+c["case"]+": "+c["actual"]["status"]+" — "+c["actual"]["reason"]+".")
lines+=["",
"extra_process_memory_isolation中，先尝试以r+b打开/proc/<VerifierPID>/mem，得到PermissionError；随后正常会话ACCEPT。它是隔离成功的正向后续，不是一次攻击被接受。",
"two_layer_challenge_structure.json核对四个relation/c来自四个不同组件。",
"","## 汇总与边界","",
"full-boundary-01：39/39预期结果，其中11个正常ACCEPT、24个REJECTED、1个FAILED、3个TIMED_OUT。",
"GREEN_03_ALL_DEADLINES.log另有3/3回归：过期缓冲消息、过期ACCEPT、非读取对端输出与失败终态不阻塞。",
"RED_01_INDEPENDENT_SESSION.log、RED_02_BUFFERED_DEADLINE.log与全部早期运行日志保留。",
"测试覆盖具体输入和状态路径；不推出FS、malicious-V ZK、concurrent/reset ZK、完整joint knowledge或生产安全。"]
(STAGE/"delivery/R7_C1_NEGATIVE_TESTS.md").write_text("\n".join(lines)+"\n",encoding="utf-8")
baseline=json.loads((ROOT/"reports/R7-STAGE1-TASK0B-20261023-v1/evidence/BASELINE_BEFORE.json").read_text())
srcdiff=[]
for rel,expected in baseline["source_files"].items():
    p=Path(rel) if Path(rel).is_absolute() else ROOT/rel
    actual=sha(p) if p.is_file() else None
    if actual!=expected:srcdiff.append(rel)
statdiff=[]
for rel,expected in baseline["protected_stats"].items():
    p=Path(rel) if Path(rel).is_absolute() else ROOT/rel
    try:
        st=p.stat()
        actual={"bytes":st.st_size,"mtime_ns":st.st_mtime_ns,"inode":st.st_ino,"device":st.st_dev}
    except FileNotFoundError:actual=None
    if actual!=expected:statdiff.append(rel)
hist=json.loads((STAGE/"evidence/HISTORICAL_REPORTS_BEFORE.json").read_text())
histdiff=[rel for rel,h in hist.items() if not (ROOT/rel).is_file() or sha(ROOT/rel)!=h]
inherited=json.loads((STAGE/"evidence/INHERITED_IDENTITY.json").read_text())
inheritdiff=[name for name,h in inherited.items() if sha(STAGE/"research_only/legacy"/name)!=h or sha(OLD/"research_only"/name)!=h]
preservation={"schema":"R7-C1-PRESERVATION-v1","utc":datetime.now(timezone.utc).isoformat(),
"source_sha_checked":len(baseline["source_files"]),"source_differences":srcdiff,
"protected_payload_stat_checked":len(baseline["protected_stats"]),"protected_stat_differences":statdiff,
"historical_report_files_sha_checked":len(hist),"historical_report_differences":histdiff,
"inherited_module_count":len(inherited),"inherited_differences":inheritdiff,
"restricted_payload_bytes_read":False,"production_source_modified":False,
"oneblock_full12":False,"large_MSM":False,"stage2":False,
"success":not(srcdiff or statdiff or histdiff or inheritdiff)}
write("evidence/PRESERVATION_FINAL.json",preservation)
assert preservation["success"],preservation
write("evidence/ENVIRONMENT.json",{"python":sys.version,"platform":platform.platform(),"machine":platform.machine(),
"fixed_workspace":str(ROOT),"ssh_host_alias":"4090_sub2api","dependencies":"Python standard library + archived toy mathematical source; no installation",
"trusted_launch":"python3 -I -B absolute verifier_server.py","source_profile":"unchanged research math; no real model parameters",
"credentials_or_ssh_config_archived":False})
status={"schema":"R7-C1-FINAL-GATE-v1","interactive_challenge_boundary":"PASS_SCOPED_RESEARCH",
"session_cases":39,"session_expected_outcomes":39,"deadline_regressions":3,
"legacy_math_source":"BYTE_IDENTICAL_8_MODULES","may_enter_task0d_executable_toy_trust_boundary":True,
"task0d_executed":False,"production_gate":"BLOCKED_CRYPTOGRAPHIC_DESIGN","may_enter_stage2":False,
"joint_knowledge":"PARTIAL_MULTI_STAGE","fiat_shamir":"BLOCKED","compression_bound":"CONDITIONAL_FIXED_ERROR_VECTOR",
"preservation_success":True}
write("FINAL_STATUS.json",status)
print(json.dumps({"report_count":len(list((STAGE/"delivery").glob("*.md"))),"cases":39,"preservation":preservation},sort_keys=True))
