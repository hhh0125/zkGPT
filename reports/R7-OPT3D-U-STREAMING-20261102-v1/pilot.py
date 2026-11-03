import pathlib,json,subprocess,os,datetime,statistics
p=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(p);s=p/'reports/R7-OPT3D-U-STREAMING-20261102-v1';tag='pilot-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ');out=s/tag;out.mkdir()
profile=json.loads((s/'PROFILE_RESULT.json').read_text());print(json.dumps({t:[round(statistics.median(x['cpu_seconds'] for x in profile['rows'] if x['prefix_bits']==t and x['phase']==j),6) for j in range(6)] for t in [0,8,16,24]}),flush=True)
rows=[]
for mode,threads in [('reference',1),('unfactored',1),('getter',1),('stream',1),('stream',2),('stream',4)]:
 c=['taskset','-c','0-3','build/opt3d-u-streaming-release/src/paper_r7/r7_opt3d_streaming_bench',mode,str(1<<20),'synthetic',str(threads)];log=out/(mode+'-'+str(threads)+'.log')
 with log.open('xb') as f:r=subprocess.run(c,stdout=f,stderr=subprocess.STDOUT,env=dict(os.environ,ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2'))
 assert r.returncode==0,log.read_text()[-2000:]
 row=json.loads(next(l.split(' ',1)[1] for l in log.read_text().splitlines() if l.startswith('OPT3D_BENCH ')));rows.append(row);(out/'RESULTS.json').write_text(json.dumps(rows,indent=2));print(json.dumps({k:row[k] for k in ['mode','threads','unified_sumcheck_seconds','cpu_seconds','U_scan_wall_seconds','U_K_product_wall_seconds']}),flush=True)
assert len({r['coefficient_challenge_endpoint_digest'] for r in rows})==1
(out/'COMPLETE.json').write_text(json.dumps(dict(success=True)))
