import hashlib,json
stage=Path('reports/R7-FULL12-20261029-v1');dest=stage/'executed-witness';dest.mkdir(exist_ok=False)
p=Path('build/release/src/paper_r7/r7_full12_witness_probe');(dest/p.name).write_bytes(p.read_bytes())
for name in ['src/paper_r7/full12_witness_probe.cpp','src/paper_r7/r6_adapter.cpp','src/paper_r7/r6_adapter.hpp','src/neuralNetwork.cpp','src/neuralNetwork.hpp','src/paper_r7/execution_target.hpp']:
 target=dest/'source'/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(Path(name).read_bytes())
(dest/'SHA256.json').write_text(json.dumps({p.relative_to(dest).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in dest.rglob('*') if p.is_file()},indent=2))
p=Path('src/paper_r7/full12_witness_probe.cpp');s=p.read_text();old='R7WitnessStore w;w.generateR6(c,t.manifest_path);';assert old in s;p.write_text(s.replace(old,'R7WitnessStore w;{master3::QuietStdout quiet;w.generateR6(c,t.manifest_path);}'))
print('EXECUTED_PROBE_BYTES_PRESERVED_AND_C_STDOUT_BOUNDARY_HARDENED')
