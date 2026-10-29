import json
d=Path('reports/R6-FULL12-P0AB-20261021/D-EXECUTION-20261022')
for n in ['FULL12_PUBLIC_X_SIGNED64_LE.bin','FULL12_FINAL_BLOCK_Y_SIGNED64_LE.bin','FULL12_FINAL_BLOCK_Y.json']:
 p=d/n;print(n,p.stat().st_size)
 if n.endswith('.json'):print(p.read_text()[:4500])
print(Path('reports/R6A/full12-model-manifest-v1.txt').read_text()[:1600])
