import pathlib,json,sys
from control import remote,ROOT
if (ROOT/'PIPELINE_STATUS.json').exists():print((ROOT/'PIPELINE_STATUS.json').read_text())
print(remote('''import pathlib,json,datetime
s=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R7-OPT3D-U-STREAMING-20261102-v1')
print(datetime.datetime.now(datetime.timezone.utc).isoformat())
for f in ['BENCHMARK_STATUS.json','BENCHMARK_GATE.json','FULL12_RUN_DECISION.json','oneblock-v1/STATUS.json','full12-v1/STATUS.json']:
 p=s/f
 if p.is_file():print(f,p.read_text())
''').decode())
