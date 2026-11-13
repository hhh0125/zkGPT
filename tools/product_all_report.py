
from pathlib import Path
import json,hashlib,collections,subprocess
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
S=ROOT/'reports/R7-PRODUCT-INTEGRATION/R7-PRODUCT-ALL-20261113-v1'
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
def objects(path,tag=None):
 out=[]
 for line in Path(path).read_text(errors='replace').splitlines():
  t=line[len(tag)+1:] if tag and line.startswith(tag+' ') else line if not tag and line.startswith('{') else ''
  if t:
   try:out.append(json.loads(t))
   except ValueError:pass
 return out
def write(name,value):(S/name).write_text(json.dumps(value,indent=2,ensure_ascii=False)+'\n')
def audit_trace(path):
 state=hashlib.sha256(b'zkGPT.R7.FS.canonical.SHA256.v1');audit=hashlib.sha256(b'zkGPT.R7.FS.challenge-audit.v1');counts=collections.Counter();seq=0
 with path.open('rb') as f:
  while True:
   raw=f.read(17)
   if not raw:break
   assert len(raw)==17
   tag=chr(raw[0]);assert tag in 'IPC' and int.from_bytes(raw[1:9],'little')==seq
   size=int.from_bytes(raw[9:17],'little');assert 0<size<=256
   label=f.read(size);length=f.read(8);assert len(label)==size and len(length)==8
   n=int.from_bytes(length,'little');assert n<=4<<20
   header=raw+label+length;state.update(header)
   if tag=='C':audit.update(header);assert n==32
   if seq<2:assert tag=='I' and label==[b'header',b'public-instance'][seq]
   left=n
   while left:
    b=f.read(min(left,4<<20));assert b;state.update(b)
    if tag=='C':audit.update(b)
    left-=len(b)
   counts[tag]+=1;seq+=1
 return {'transcript_digest':state.hexdigest(),'challenge_digest':audit.hexdigest(),'events':seq,'prover_messages':counts['P'],'challenges':counts['C'],'bytes':path.stat().st_size,'sha256':sha(path)}
def main():
 assert json.loads((S/'FULL12_RESULT.json').read_text())['success']
 stage=S/'full12-all-optimized';run=json.loads((stage/'RESULT.json').read_text());assert run['success']
 trace=audit_trace(stage/'P-EVENTS.bin');assert trace['sha256']==sha(stage/'V-EVENTS.bin')
 for key in ['transcript_digest','challenge_digest','events','prover_messages','challenges']:assert trace[key]==run['fs'][key],key
 write('TRACE_AUDIT.json',trace)
 result={}
 for side in ['P','V']:
  log=ROOT/'logs'/S.name/('full12-all-optimized-'+side+'.log')
  products=objects(log,'PERF5_PRODUCT');scopes=objects(log,'PERF4_SCOPE');groups={}
  for i in range(3):
   rows=[p for p in products if p['role'].endswith('/product/'+str(i))];assert rows
   regions={}
   for name in rows[0]['regions']:
    v={key:sum(p['regions'][name][key] for p in rows) for key in rows[0]['regions'][name]}
    v['average_wall_time']=v['inclusive_wall']/v['calls'] if v['calls'] else 0;regions[name]=v
   groups[str(i)]={'calls':len(rows),'regions':regions,'optimization':{k:sum(p['optimization'][k] for p in rows) for k in rows[0]['optimization']},
    'weights':{k:sum(p['weights'][k] for p in rows) for k in rows[0]['weights']},
    'leaf_evaluate_weights':{k:sum(p['leaf_evaluate_weights'][k] for p in rows) for k in rows[0]['leaf_evaluate_weights']}}
  total=sum(g['regions']['total']['inclusive_wall'] for g in groups.values())
  for g in groups.values():g['share_percent']=100*g['regions']['total']['inclusive_wall']/total
  sums={}
  for kind in sorted({x['kind'] for x in scopes}):
   rows=[x for x in scopes if x['kind']==kind]
   sums[kind]={'calls':len(rows),**{k:sum(x[k] for x in rows) for k in ['inclusive_wall','exclusive_wall','allocations','allocated_bytes']}}
  phases=[x['phases'] for x in objects(log) if 'phases' in x];assert len(phases)==1
  result[side]={'products':groups,'scopes':sums,'phases':phases[0],'product_wall':total,'process':run[side]}
 g=result['P']['products'];prep=result['P']['scopes']['Native/Product01-base-leaves']
 assert prep['calls']==g['0']['calls']==g['1']['calls']
 for i in ['0','1']:assert g[i]['regions']['tree_leaf_callback']['calls']==0 and g[i]['optimization']['prepared_leaf_reuses']==g[i]['calls']
 for v in result['V']['products'].values():assert v['weights']['calls']==v['leaf_evaluate_weights']['calls']==0
 write('FULL12_STATISTICS.json',result)
 before=json.loads((S/'BEFORE_SOURCE.json').read_text());frozen=json.loads((S/'FROZEN_SOURCE.json').read_text());parameters=json.loads((S/'PARAMETERS.json').read_text())
 identity={'source_tree_sha256':hashlib.sha256(json.dumps(frozen,sort_keys=True,separators=(',',':')).encode()).hexdigest(),'source_manifest_file_sha256':sha(S/'FROZEN_SOURCE.json'),
 'parameter_manifest_file_sha256':sha(S/'PARAMETERS.json'),'parameter_tree_sha256':hashlib.sha256(json.dumps(parameters,sort_keys=True,separators=(',',':')).encode()).hexdigest(),
 'parameter_files':len(parameters),'changed_source_files':[k for k in frozen if frozen[k]!=before.get(k)],'ancestor_git_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()}
 write('FINAL_IDENTITY.json',identity)
 lines=[];add=lines.append
 add('# R7 Product 全部优化集成与 Full12 性能验收\n')
 add('固定工作目录当前源码已完成集成；只运行一次全部优化开启的Full12，独立Verifier接受，coverage %s/%s。完整CTest 121/122，唯一保留失败为已知r7_zero_suffix_storage，不能称全套全绿。没有旧版Full12、性能A/B或规模扫描。\n'%(run['coverage_count'],run['coverage_count']))
 add('## A. 修改与计时位置\n')
 add('\n'.join('- '+p for p in identity['changed_source_files']))
 add('\n新增工具：tools/product_all_remote.py、product_all_package.py、product_all_report.py；source.patch与before/保留本轮差异和修改前字节。')
 add('A2/C2为getenv读取的运行期开关，正式环境ZKGPT_PERF6_A2=1、ZKGPT_PERF6_C2=1；不是CMake宏。Release、4线程、MALLOC_ARENA_MAX=2、固定四核affinity详见FINAL_CONFIG。原模型/输入/statement/参数不变；用户确认正式Full12沿用SessionRandom/OSRandom系统熵，固定seed只用于回归。')
 add('oneblock_lookup 保留单次fingerprint、base_leaves、Product0的RAII加一/恢复及Product1复用；ProductInputs借用现有vectors。allow_specialized在Product2显式false，保持原通用多项式/callback首轮折叠。A2专用核配合C2首轮数组读取，后续轮在已有连续folded vectors原地折叠。')
 add('新增PERF5 Recurrence（嵌套于旧Messages）、ProductInputs构造；新增PERF4 Product0-add-one/restore、Product2-root-leaves；其余复用既有计时。每次Product汇总，无逐元素/逐轮文本日志。可选缓冲canonical trace输出不改变哈希输入、Proof framing、消息、挑战或熵。\n')
 add('## B. 优化启用状态\n\n| 优化 | 代码位置 | 运行证据 |\n|---|---|---|')
 add('| Prover Weights复用 | evaluateWeights/evaluateWithWeights | P0/P1各%s/%s次权重生成，每次供三个源MLE复用 |'%(g['0']['leaf_evaluate_weights']['calls'],g['1']['leaf_evaluate_weights']['calls']))
 add('| Verifier无用Weights删除 | hiddenProduct/evaluate | 三类Product layer/leaf weights调用均0 |')
 add('| 共享叶子 | hiddenMemory/hiddenProduct/AddOneGuard | %s次共同准备，P0/P1各%s/%s次prepared reuse，TreeLeaf callback均0 |'%(prep['calls'],g['0']['optimization']['prepared_leaf_reuses'],g['1']['optimization']['prepared_leaf_reuses']))
 add('| A2 | productSumcheck/productPolynomial | P0/P1各%s/%s次实际调用，P2为0 |'%(g['0']['optimization']['a2_calls'],g['1']['optimization']['a2_calls']))
 add('| C2 | ProductInputs/productPolynomialContiguous/foldProductInputs | P0/P1各%s/%s次直接折叠，P2为0，输入拷贝字节0 |'%(g['0']['optimization']['c2_fold_calls'],g['1']['optimization']['c2_fold_calls']))
 add('\n权重复用在每次Product的address/value/read三个MLE之间。Product0与Product1的挑战不同，不能跨二者错误复用同一权重向量。准备一次是每个Memory一次，并非整个Full12总共一次。\n')
 add('## C. Full12顶层实际性能\n\n| 阶段 | 秒 | 口径 |\n|---|---:|---|')
 phases={p['name']:p['seconds'] for p in result['P']['phases']}
 for label,name in [('Witness','frozen-witness-and-prechallenge-lookup'),('Seal','all-classified-roots-prechallenge-seal'),('GKR','all-native-GKR'),('Native','native-Range-Exp-Lasso-Memory-Product'),('Handles','all-source-original-handles'),('Unified Sumcheck + Opening','protocol1-unified-input-sumcheck-and-opening')]:
  if name not in phases:
   if label=='Native':
    candidates=[k for k in phases if ('native' in k or 'Range' in k or 'lookup' in k) and 'GKR' not in k and 'witness' not in k];assert len(candidates)==1,candidates;name=candidates[0]
   else:raise AssertionError((label,list(phases)))
  add('| %s | %.6f | inclusive：%s |'%(label,phases[name],name))
 add('| Product | %.6f | hiddenProduct inclusive合计，包含于Native，不含外部准备 |'%result['P']['product_wall'])
 add('| Prover Total | %.6f | 整个进程wall，含启动/完成和trace输出 |'%run['P']['wall_seconds'])
 add('| Verifier Total | %.6f | 独立进程wall |'%run['V']['wall_seconds'])
 add('\nNative包含Product；Unified包含Sumcheck/Opening，不能与子阶段重复加总。Witness使用既有witness及prechallenge helper准备边界；Seal是原Commit Seal边界。所有原始顶层和PERF4 inclusive/exclusive均保留于JSON。\n')
 add('## D. Product内部实际性能\n\n| 操作（秒） | Product0 | Product1 | Product2 |\n|---|---:|---:|---:|')
 for label,name,key in [('Polynomial','polynomial_callback_arithmetic_reduction','inclusive_wall'),('Messages','messages_and_recurrence_verification','exclusive_wall'),('Recurrence','recurrence_verification','inclusive_wall'),('Leaf Authentication','leaf_authentication','exclusive_wall'),('Fold','fold_callback_arithmetic','inclusive_wall'),('Tree Parent','tree_parent','inclusive_wall'),('ProductInputs构造','product_inputs_construct','inclusive_wall'),('Tree Leaf callback准备','tree_leaf_callback','inclusive_wall'),('Product Total','total','inclusive_wall')]:
  add('| '+label+' | '+' | '.join('%.6f'%g[str(i)]['regions'][name][key] for i in range(3))+' |')
 add('| Weights（layer+leaf） | '+' | '.join('%.6f'%(g[str(i)]['regions']['weights']['inclusive_wall']+g[str(i)]['regions']['leaf_evaluate_weights']['inclusive_wall']) for i in range(3))+' |')
 add('\n| 内部阶段 | P0 calls / inclusive avg秒 | P1 calls / inclusive avg秒 | P2 calls / inclusive avg秒 |\n|---|---:|---:|---:|')
 for name in g['0']['regions']:
  if name=='total':continue
  add('| '+name+' | '+' | '.join('%s / %.9f'%(g[str(i)]['regions'][name]['calls'],g[str(i)]['regions'][name]['average_wall_time']) for i in range(3))+' |')
 add('\n主耗时矩阵Messages为exclusive，排除Recurrence；调用次数/平均表使用inclusive平均时间，不能与矩阵exclusive值混用；Recurrence包含该关系的构造及Sigma证明/验证，按P/V角色分别记录；Leaf Authentication为exclusive，排除其leaf weights。Product Total为inclusive，另含expression/challenge/endpoint/equality/carry/coverage/alloc/free等阶段。所有阶段调用数、inclusive/exclusive/average时间和分配统计见FULL12_STATISTICS.json。\n')
 add('| 外部准备（在Native内，Product Total之外） | 次数 | 秒 |\n|---|---:|---:|')
 for name in ['Native/Product01-base-leaves','Native/Product0-add-one','Native/Product0-restore','Native/Product2-root-leaves']:
  x=result['P']['scopes'][name];add('| %s | %s | %.6f |'%(name,x['calls'],x['inclusive_wall']))
 add('\nP0/P1 fingerprint/prepared leaves共同准备单列，不重复分摊。P2根扫描与其TreeLeaf callback为保留的两个位置。ProductInputs仅引用绑定，没有另一次payload准备/复制，实际输入拷贝字节0，构造时间已列。\n')
 add('| Product | 调用次数 | 平均秒 | Product占比 | 普通new次数 | 累计申请字节 |\n|---|---:|---:|---:|---:|---:|')
 for i in range(3):
  x=g[str(i)];z=x['regions']['total'];add('| %s | %s | %.6f | %.3f%% | %s | %s |'%(i,x['calls'],z['inclusive_wall']/x['calls'],x['share_percent'],z['allocations'],z['allocated_bytes']))
 add('\nAllocationTrace只统计当前线程普通C++ new/new[]累计请求，不含malloc/aligned/其他线程，不等于净存活/RSS；嵌套分配不能直接相加。参考次数是每类104；实际次数取日志，未强行改成参考值。\n')
 add('| Product | A2调用 | A2元素对 | 消除系数乘法 | C2多项式调用 | C2折叠调用 | C2折叠元素对 | 输入构造 |\n|---|---:|---:|---:|---:|---:|---:|---:|')
 for i in range(3):
  x=g[str(i)]['optimization'];add('| %s | %s | %s | %s | %s | %s | %s | %s |'%(i,x['a2_calls'],x['a2_pairs'],x['a2_coefficient_multiplications_eliminated'],x['c2_polynomial_calls'],x['c2_fold_calls'],x['c2_fold_pairs'],x['input_constructions']))
 add('\nA2每元素对消除通用单位系数4次乘法（专用10次、通用14次），由实测对数计算确定工作量，不表示墙钟节省。C2直接数组核在每个非单点层首轮执行，后续轮在已有连续folded vectors原地fold，无callback回退。首轮多项式和折叠各消除6次间接factor读取/元素对，不据此推算DRAM流量或加速比。\n')
 add('## E. 正确性验收与身份\n')
 add('- Release全量构建通过；Product 6/6、oneblock 2/2；完整CTest121/122，当前与保留旧二进制复现同一storage分配预算失败，预算/阈值/断言未改。')
 add('- ASan/UBSan相关测试见asan-tests.log/json，无新错误。确定性通用/优化Proof、完整P/V event tape及canonical trace字节相同；覆盖0/1/负一边界、全域确定性输入、逐轮fold、正常/异常RAII及n=1。')
 add('- Full12只运行一次；独立Verifier读取正式Proof，P/V metadata与完整canonical事件一致，TRACE_AUDIT从完整trace重新计算Transcript/Challenge digest和事件计数。')
 add('- Proof %s字节；SHA256 %s。'%(run['proof_bytes'],run['proof_sha256']))
 add('- 原始完整日志 logs/%s/full12-all-optimized-P.log与-V.log；本阶段full12-all-optimized/保存proof/statement/P-V完整trace/coverage/layout。'%S.name)
 add('- Source tree SHA256 '+identity['source_tree_sha256']+'；source manifest文件SHA256 '+identity['source_manifest_file_sha256']+'。')
 add('- Source tree沿用现有R9冻结机制，范围为src/全部文件及根CMakeLists.txt；MCL/cnpy、编排工具、配置、报告与全部归档成员另有逐文件SHA，不能将此冻结摘要误作整个OS或Git commit。')
 add('- 参数manifest文件SHA256 '+identity['parameter_manifest_file_sha256']+'，%s项前后匹配；参数tree SHA256 '%len(parameters)+identity['parameter_tree_sha256']+'。')
 add('- 祖先Git HEAD '+identity['ancestor_git_head']+'；R7为未提交源码快照，该HEAD不能代替源码身份。BEFORE_SOURCE/FROZEN_SOURCE/source.patch/Git状态均保留；未commit/push/reset/clean。')
 add('- 实际二进制/依赖/配置SHA见FINAL_CONFIG、full12-all-optimized/ENVIRONMENT和identity-FINAL。已有production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN、may_enter_stage2=false未改变；本轮工程验收不改变此历史状态。\n')
 add('## F. 实测瓶颈判断\n')
 combined=sum(g[k]['regions']['total']['inclusive_wall'] for k in ['0','1']);p2=g['2']['regions']['total']['inclusive_wall']
 add('Product0/1合计%.6f秒（%.3f%%），Product2 %.6f秒（%.3f%%）；%s。'%(combined,100*combined/result['P']['product_wall'],p2,100*p2/result['P']['product_wall'],'Product0/1仍占主要耗时' if combined>p2 else 'Product2占主要耗时'))
 for i in range(3):
  rank=sorted(((n,z['exclusive_wall']) for n,z in g[str(i)]['regions'].items() if n!='total'),key=lambda x:x[1],reverse=True)
  add('Product%s最高exclusive子阶段：%s。'%(i,'；'.join('%s %.6fs'%x for x in rank[:3])))
 add('共享叶子消除重复fingerprint扫描与TreeLeaf复制；不同挑战下源MLE/Opening仍需执行，不能宣称认证工作消失。每次叶认证三个MLE已复用weights；跨P0/P1挑战不同，本次没有证明可进一步跨调用安全复用。层weights递推可另行评估，不在本轮范围。')
 worst=max(range(3),key=lambda i:g[str(i)]['regions']['total']['inclusive_wall'])
 add('按占比优先评估%s，并针对其上列最高阶段选择具体函数：Polynomial对应productPolynomial/productPolynomialContiguous，Fold对应foldProductInputs/原地fold，Leaf Authentication对应evaluateWithWeights/evaluate/auth，Messages/Recurrence对应exchangeHidden/sigma。不能将本轮数据解释为旧版加速百分比。'%('Product0/1共同路径（两者耗时接近）' if worst in [0,1] else 'Product2'))
 for i in [0,1]:
  z=g[str(i)]['regions'];rank=sorted(((n,x['exclusive_wall']) for n,x in z.items() if n!='total'),key=lambda x:x[1],reverse=True)
  kernel=z['polynomial_callback_arithmetic_reduction']['exclusive_wall']+z['fold_callback_arithmetic']['exclusive_wall']
  functions={'polynomial_callback_arithmetic_reduction':'productPolynomial / productPolynomialContiguous','fold_callback_arithmetic':'foldProductInputs 与后续原地fold循环','leaf_authentication':'evaluateWithWeights 与 LeafAuthenticator 回调','messages_and_recurrence_verification':'exchangeHidden','recurrence_verification':'sigma（recurrence relation）','endpoint_authentication':'endpoint / sigma','weights':'r1::weights（Product layer）','leaf_evaluate_weights':'evaluateWeights'}
  add('Product%s 的 Polynomial+Fold 合计 %.6f秒，占该Product %.3f%%；按最大exclusive阶段，本次最具体的继续优化候选是 %s。'%(i,kernel,100*kernel/z['total']['inclusive_wall'],functions.get(rank[0][0],rank[0][0])))
 add('Product2是否优先：'+('本次占比已超过单个Product0或1，应列入后续重点。' if p2>max(g['0']['regions']['total']['inclusive_wall'],g['1']['regions']['total']['inclusive_wall']) else '本次耗时低于更重的Product0/1，优先处理实测更重的阶段；Product2仍可按其独立消息/认证占比评估。'))
 add('\n## 失败证据、审查和归档\n')
 add('preliminary-build/build-attempt1/build-attempt2/test-attempt1/FAILURE-*完整保留补丁转义、宏包含默认参兼容和测试profiler上下文冲突的失败与修正证据；Full12未中途失败或静默重跑。独立审查的重要意见已修复：验证结果绑定源码/二进制，运行计数非空、角色顺序检查，启动前VALIDATION_SEAL核对。')
 add('Windows PREFLIGHT与FINAL归档包含完整源码字节、before/patch、MCL/cnpy、构建与依赖身份、参数身份、statement/proof、完整trace、原始验证/测试/失败与资源日志。容器和逐成员SHA、gzip CRC及实际恢复读取由对应LOCAL_VERIFICATION.json确认；未验证时不算完成。大型未变模型数组按参数manifest复用现有保留数据，不声称包含完整OS/全部模型数组。本次无远程删除。')
 (S/'FINAL_REPORT.zh-CN.md').write_text('\n'.join(lines)+'\n')
 print(json.dumps({'P':run['P']['wall_seconds'],'V':run['V']['wall_seconds'],'products':{k:{'calls':v['calls'],'seconds':v['regions']['total']['inclusive_wall'],'a2':v['optimization']['a2_calls'],'c2':v['optimization']['c2_fold_calls']} for k,v in g.items()},'source_tree_sha256':identity['source_tree_sha256']},indent=2))
if __name__=='__main__':main()
