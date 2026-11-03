from control import *
print(remote('''import pathlib,json
s=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R7-OPT3D-U-STREAMING-20261102-v1')
state=json.loads((s/'BENCHMARK_STATUS.json').read_text());d=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')/state['directory']
assert (s/'INITIAL_BENCH_STOP.json').is_file() and not (d/'COMPLETE.json').exists()
(d/'PILOT_STATUS.json').write_text(json.dumps(dict(status='STOPPED_RETAINED_EXCLUDED',reason='required allocation/fused-byte telemetry added; all partial rows preserved, excluded from final formal gate',prior_rows=len(json.loads((d/'RESULTS.json').read_text()))),indent=2))
print('Initial partial benchmark retained for archive',d)
''').decode())
dump('PIPELINE_INITIAL_FAILED_RETAINED.json',json.loads((ROOT/'PIPELINE_STATUS.json').read_text()))
