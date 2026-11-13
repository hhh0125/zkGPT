from pathlib import Path
import json,csv,collections,statistics,hashlib
S=Path(__file__).resolve().parent;ROOT=S.parent.parent;STAGE=S/'full12-R9-profiled'
def load(p):return json.loads(p.read_text())
def save(name,v):(S/name).write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')
def objects(path,tag=None):
 out=[]
 for line in path.read_text(errors='replace').splitlines():
  raw=line[len(tag)+1:] if tag and line.startswith(tag+' ') else line if not tag and line.startswith('{') else ''
  if raw:
   try:out.append(json.loads(raw))
   except ValueError:pass
 return out
def tsv(path,rows):
 if not rows:return
 with path.open('w') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]),delimiter='\t',extrasaction='ignore');w.writeheader();w.writerows(rows)
def summarize_resources(role):
 observations=[json.loads(l) for l in (STAGE/(role+'-RESOURCE-SAMPLES.jsonl')).read_text().splitlines()];loads=[x['load'][0] for x in observations];freqs=[f for x in observations for f in x['cpu_frequency_khz'].values() if f is not None];return {'samples':len(observations),'system_load_1m':{'min':min(loads),'median':statistics.median(loads),'max':max(loads)},'sampled_scaling_frequency_khz':{'min':min(freqs),'median':statistics.median(freqs),'max':max(freqs)} if freqs else None,'warning':'Host load/frequency samples do not by themselves establish contention or causality; proc statistics and schedstat are preserved in raw JSONL.'}
def main():
 assert load(S/'EXPERIMENTS_RESULT.json')['success'];r=load(STAGE/'RESULT.json');phases={};groups={};resource={};coverage=r['coverage_count'];lines=['# Full12 重跑与全流程计时','','当前 R9 Release 程序，源码/参数不变；开启已有 ZKGPT_PERF4_PROFILE=1，不修改协议或线程。Native 四组以现有函数范围定义，存在嵌套，inclusive 不能直接相加；exclusive 为扣除被测子调用后的自身时间。Native/Fingerprint-setup 的 exclusive 时间归入 Lasso。','', 'production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN；may_enter_stage2=false。','']
 for role in ['P','V']:
  log=ROOT/r[role]['log'];summary=[x for x in objects(log) if 'phases' in x];assert len(summary)==1
  cpus={x['name']:x['cpu_seconds'] for x in objects(log,'PHASE_CPU')};rows=[]
  for phase in summary[0]['phases']:rows.append(dict(name=phase['name'],wall_seconds=phase['seconds'],cpu_seconds=cpus[phase['name']],P_wire_bytes=phase['P_wire_bytes'],MSM_calls=phase['MSM_calls'],rss_end_kib=phase.get('rss_end_kib'),cumulative_peak_rss_kib=phase.get('cumulative_peak_rss_kib')))
  fullwall=r[role]['wall_seconds'];fullcpu=r[role]['user_cpu_seconds']+r[role]['system_cpu_seconds'];phases[role]=dict(process_wall_seconds=fullwall,process_cpu_seconds=fullcpu,measured_phases=rows,outside_named_phases_wall_seconds=fullwall-sum(x['wall_seconds'] for x in rows),outside_named_phases_cpu_seconds=fullcpu-sum(x['cpu_seconds'] for x in rows));tsv(S/(role+'-PHASE_TIMES.tsv'),rows)
  scopes=objects(log,'PERF4_SCOPE');tsv(S/(role+'-SCOPES.tsv'),scopes)
  aggregated={}
  for scope in scopes:
   if not scope['kind'].startswith('Native/'):continue
   item=aggregated.setdefault(scope['kind'],dict(calls=0,inclusive_wall=0.,exclusive_wall=0.,inclusive_cpu=0.,exclusive_cpu=0.));item['calls']+=1
   for k in ['inclusive_wall','exclusive_wall','inclusive_cpu','exclusive_cpu']:item[k]+=scope[k]
  expected={'Native/Range','Native/Exp','Native/Memory','Native/Product','Native/Fingerprint-setup'};assert expected<=aggregated.keys(),aggregated.keys();groups[role]=aggregated
  groupRows=[]
  for name,key in [('Range','Native/Range'),('Exp','Native/Exp'),('Lasso (Memory checks)','Native/Memory'),('Memory/Product (recursive Product)','Native/Product')]:
   v=aggregated[key].copy()
   if key=='Native/Memory':
    for k in ['exclusive_wall','exclusive_cpu']:v[k]+=aggregated['Native/Fingerprint-setup'][k]
   groupRows.append(dict(group=name,**v))
  tsv(S/(role+'-NATIVE_FOUR_GROUPS.tsv'),groupRows)
  native=next(x for x in rows if x['name']=='native-Range-Exp-Lasso-Memory-Product');groups[role]['four_group_exclusive_wall_sum']=sum(x['exclusive_wall'] for x in groupRows);groups[role]['native_phase_wall']=native['wall_seconds'];groups[role]['unattributed_wall_including_logging_and_scope_boundaries']=native['wall_seconds']-groups[role]['four_group_exclusive_wall_sum']
  byroot=collections.defaultdict(lambda:dict(calls=0,inclusive_wall=0.,exclusive_wall=0.,inclusive_cpu=0.,exclusive_cpu=0.))
  for x in scopes:
   if not x['kind'].startswith('Native/'):continue
   path=x['role'];top=path;v=byroot[(x['kind'],top)];v['calls']+=1
   for k in ['inclusive_wall','exclusive_wall','inclusive_cpu','exclusive_cpu']:v[k]+=x[k]
  tsv(S/(role+'-NATIVE_BY_ROLE.tsv'),[dict(kind=k[0],role=k[1],**v) for k,v in sorted(byroot.items())])
  resource[role]=summarize_resources(role)
  lines += [f'## {role} 全流程','','墙钟时间是实际经过时间，进程 CPU 时间累计各线程，二者不可混用。','','| 阶段 | 墙钟秒 | CPU 秒 |','|---|---:|---:|']
  for x in rows:lines.append(f"| {x['name']} | {x['wall_seconds']:.6f} | {x['cpu_seconds']:.6f} |")
  lines += [f"| 未包含在命名阶段中的启动/间隙/收尾 | {phases[role]['outside_named_phases_wall_seconds']:.6f} | {phases[role]['outside_named_phases_cpu_seconds']:.6f} |",f'| 进程总计 | {fullwall:.6f} | {fullcpu:.6f} |','',f'### {role} Native 四部分','','inclusive 包含嵌套子调用；四部分占比/汇总使用 exclusive。Lasso 行 inclusive 已包含 Fingerprint-setup，exclusive 将其加回。Memory/Product 行对应 hiddenProduct 递归乘积证明；Lasso/Memory 与 Product 并非两个顺序独立模块。','','| 部分 | 调用次数 | inclusive 墙钟秒 | exclusive 墙钟秒 | inclusive CPU 秒 | exclusive CPU 秒 |','|---|---:|---:|---:|---:|---:|']
  for x in groupRows:lines.append(f"| {x['group']} | {x['calls']} | {x['inclusive_wall']:.6f} | {x['exclusive_wall']:.6f} | {x['inclusive_cpu']:.6f} | {x['exclusive_cpu']:.6f} |")
  lines += ['',f"四组 exclusive wall 合计 {groups[role]['four_group_exclusive_wall_sum']:.6f} 秒；native 顶层 {native['wall_seconds']:.6f} 秒；差额 {groups[role]['unattributed_wall_including_logging_and_scope_boundaries']:.6f} 秒包含 scope 输出、边界和未覆盖开销。",'']
 save('PHASE_TIMES.json',phases);save('NATIVE_SCOPE_SUMMARY.json',groups);save('RESOURCE_SUMMARY.json',resource)
 historical={}
 for label,p in [('R8_Immediate_historical',ROOT/'reports/R8-U-FOLD-20261111-v1/full12-immediate/RESULT.json'),('R9_previous',ROOT/'reports/R9-IMMEDIATE-ONLY-U-20261111-v1/full12-R9/RESULT.json')]:
  old=load(p);historical[label]={}
  for role in ['P','V']:
   oldphases=[x for x in objects(ROOT/old[role]['log']) if 'phases' in x][0]['phases'];historical[label][role]=dict(process_wall_seconds=old[role]['wall_seconds'],phases={x['name']:x['seconds'] for x in oldphases})
 save('HISTORICAL_TIMES.json',historical);lines+=['## 历史时间参考','','本次开启 scope profiling，有输出和计量开销；历史运行没有该细分 profile。且只有单次 Full12，不能据此作严格性能因果结论。','','| 阶段（Prover） | R8 Immediate 历史 | 上次 R9 | 本次 R9 profile |','|---|---:|---:|---:|']
 for x in phases['P']['measured_phases']:
  name=x['name'];lines.append(f"| {name} | {historical['R8_Immediate_historical']['P']['phases'].get(name,0):.6f} | {historical['R9_previous']['P']['phases'].get(name,0):.6f} | {x['wall_seconds']:.6f} |")
 lines += [f"| Prover 总计 | {historical['R8_Immediate_historical']['P']['process_wall_seconds']:.6f} | {historical['R9_previous']['P']['process_wall_seconds']:.6f} | {r['P']['wall_seconds']:.6f} |",'', '## 验证和证据','',f"Full12 Prover/独立 Verifier 成功，coverage {coverage} 全部通过；P/V proof、transcript、challenge digest 相同。Proof {r['proof_bytes']} bytes，SHA256 `{r['proof_sha256']}`。",'', '源码 FROZEN_SOURCE.json 与上次 R9 完全一致，参数逐文件核对；未重新编译或改源码，当前 Release 二进制哈希记录在 stage/ENVIRONMENT.json。每个 Native 调用的原始 scope 与按角色汇总见 P/V-SCOPES.tsv、P/V-NATIVE_BY_ROLE.tsv。资源采样见 full12-R9-profiled/P/V-RESOURCE-SAMPLES.jsonl，汇总见 RESOURCE_SUMMARY.json；整机负载不能替代四核争用测量，频率样本也不是进程实际指令吞吐测量。','', '所有历史报告保持不变。工程复测不解除密码学设计门禁。']
 (S/'FINAL_REPORT.zh-CN.md').write_text('\n'.join(lines)+'\n');save('STATUS.json',dict(success=True,full12_verified=True,source_unchanged=True,production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False))
if __name__=='__main__':main()
