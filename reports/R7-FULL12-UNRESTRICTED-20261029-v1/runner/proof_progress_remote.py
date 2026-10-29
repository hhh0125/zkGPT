import json,struct,time
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1');final=stage/'full12-v1.fsproof'
paths=[final] if final.exists() else list(stage.glob('full12-v1.fsproof.partial.*'))
assert len(paths)==1
path=paths[0];stat=path.stat();state_path=stage/'PUBLIC_PROOF_WRITE_PROGRESS.json'
state=json.loads(state_path.read_text()) if state_path.exists() else {}
with path.open('rb') as f:
 if state.get('inode')==stat.st_ino:at=state['complete_offset']
 else:
  raw=f.read(12);assert raw[:8]==b'ZKR7FS1\0';at=12+int.from_bytes(raw[8:12],'little');state=dict(inode=stat.st_ino,complete_offset=at,complete_file_messages=0,last_label=None)
 while at+25<=stat.st_size:
  f.seek(at);head=f.read(17);assert head[:1] in [b'P',b'F'];label_len=int.from_bytes(head[9:17],'little');assert 0<label_len<=256
  if at+25+label_len>stat.st_size:break
  label=f.read(label_len).decode();size=int.from_bytes(f.read(8),'little');assert size<=4<<20
  end=at+25+label_len+size
  if end>stat.st_size:break
  at=end;state.update(complete_offset=at,complete_file_messages=state['complete_file_messages']+1,last_label=label,last_sequence=int.from_bytes(head[1:9],'little'))
state.update(proof_file=path.name,bytes=stat.st_size,timestamp_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()))
state_path.write_text(json.dumps(state,indent=2)+'\n')
role='full-V' if (stage/'full-V-resources.jsonl').exists() and (stage/'full-V-resources.jsonl').stat().st_size else 'full-P'
sample=json.loads((stage/(role+'-resources.jsonl')).read_text().splitlines()[-1])
lines=(stage/(role+'.log')).read_text().splitlines();phases=[line for line in lines if line.startswith('PHASE_BEGIN ')]
complete=json.loads((stage/'FULL12_EXECUTION_COMPLETE.json').read_text()) if (stage/'FULL12_EXECUTION_COMPLETE.json').exists() else None
cpu=None
try:
 fields=Path('/proc/'+str(sample['pid'])+'/stat').read_text().split(') ',1)[1].split();cpu=(int(fields[11])+int(fields[12]))/os.sysconf('SC_CLK_TCK')
except FileNotFoundError:pass
print(json.dumps(dict(active_role=role,last_label=state['last_label'],proof_bytes=state['bytes'],complete_messages=state['complete_file_messages'],timestamp_utc=state['timestamp_utc'],phase=phases[-1] if phases else None,last_runtime_line=lines[-1] if lines else None,current_process_cpu_seconds=cpu,resource=sample,complete=complete)),flush=True)
