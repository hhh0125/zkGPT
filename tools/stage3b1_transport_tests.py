"""Adversarial file-backed extraction checks; no network or cryptography."""
from pathlib import Path
import io,tarfile,tempfile,unittest
from r7_sync_safety import extract_stage,extract_stage3b1_file
class Stage3B1TransportTests(unittest.TestCase):
 def fixture(self,root,entries):
  archive=root/'public.tar.gz'
  with tarfile.open(archive,'w:gz') as t:
   for name,data,kind in entries:
    m=tarfile.TarInfo(name)
    if kind=='link':m.type=tarfile.SYMTYPE;m.linkname='../../protected'
    else:m.size=len(data)
    t.addfile(m,None if kind=='link' else io.BytesIO(data))
  return archive
 def test_exact_stage_and_content(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d).resolve();dest=root/'R7-STAGE3B1-RUN-fixture';dest.mkdir();name='reports/'+dest.name+'/wire/00000.frame';payload=b'public-masked-response'*200000
   archive=self.fixture(root,[(name,payload,'file')]);extract_stage3b1_file(archive,dest);self.assertEqual((dest/'wire/00000.frame').read_bytes(),payload)
 def test_escape_and_other_stage(self):
  for tail in ['../../protected','../R7-STAGE3B1-RUN-other/file']:
   with self.subTest(tail=tail),tempfile.TemporaryDirectory() as d:
    root=Path(d).resolve();dest=root/'R7-STAGE3B1-RUN-fixture';dest.mkdir();archive=self.fixture(root,[('reports/'+dest.name+'/'+tail,b'bad','file')])
    with self.assertRaises(ValueError):extract_stage3b1_file(archive,dest)
    self.assertFalse((root/'protected').exists())
 def test_links_duplicates_and_member_budget(self):
  for case in ['link','duplicate','large']:
   with self.subTest(case=case),tempfile.TemporaryDirectory() as d:
    root=Path(d).resolve();dest=root/'R7-STAGE3B1-RUN-fixture';dest.mkdir();name='reports/'+dest.name+'/wire.frame';entries=[(name,b'x'*(8*1024*1024+1) if case=='large' else b'bad','link' if case=='link' else 'file')]
    if case=='duplicate':entries.append(entries[0])
    archive=self.fixture(root,entries)
    with self.assertRaises(ValueError):extract_stage3b1_file(archive,dest)
    self.assertFalse((dest/'wire.frame').exists(),'prevalidation must precede writes')
 def test_ordinary_extractor_stays_separate(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d).resolve();dest=root/'R7-STAGE3B1-RUN-fixture';dest.mkdir();archive=self.fixture(root,[('reports/'+dest.name+'/public',b'ok','file')])
   with self.assertRaises(ValueError):extract_stage(archive.read_bytes(),dest)
if __name__=='__main__':unittest.main(verbosity=2)
