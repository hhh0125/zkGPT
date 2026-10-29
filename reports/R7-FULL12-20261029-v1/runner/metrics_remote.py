import json
p=json.loads(Path('reports/R7-FULL12-20261029-v1/native-partitioned/R7_FULL12_PARTITIONS.json').read_text())
print({k:p[k] for k in ['master_actual','dense_mapping_bytes']})
print('roots',len(p['roots']),'model',sum(r['model'] for r in p['roots']),'base_runs',len(p['base_coordinate_runs']))
print('actual_total',sum(r['actual'] for r in p['roots']),'padded_total',sum(r['padded'] for r in p['roots']))
print('native_helper_max',max(v['padded'] for v in p['views'] if not v['mapped']))
