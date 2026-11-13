from product_leaves_remote import *
for label,binary in [('isolated-candidate',BIN/'r7_zero_suffix_storage_tests'),('retained-release',ROOT/'build/release/src/paper_r7/r7_zero_suffix_storage_tests')]:
 if binary.is_file():
  r=run('storage-'+label,[binary],expected=1)
  assert 'R7 source allocation still materializes padded witness suffix' in Path(r['log']).read_text()
