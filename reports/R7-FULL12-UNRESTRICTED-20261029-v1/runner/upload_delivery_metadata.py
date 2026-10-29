from pathlib import Path
import subprocess,json,hashlib,base64
root=Path(__file__).resolve().parent.parent;repo=root/'zkGPT-R7-dev';scripts=Path(__file__).resolve().parent
def git(*args):return subprocess.check_output(['git','-c','safe.directory='+repo.as_posix(),'-C',str(repo),*args])
head=git('rev-parse','HEAD').decode().strip();source_files=json.loads((scripts/'review/IDENTITY.json').read_text())['current']
assert all(hashlib.sha256((repo/n).read_bytes()).hexdigest()==h for n,h in source_files.items())
payload={'FINAL_SOURCE_COMMIT.json':(json.dumps(dict(source_commit=head,authority='Windows zkGPT-R7-dev',source_files=386,baseline_head='9f8869e304ffff4db3a0b13bbb2dac68427fba24'),indent=2)+'\n').encode(),'SOURCE_BASELINE_TO_FINAL.patch':git('diff','--binary','9f8869e304ffff4db3a0b13bbb2dac68427fba24',head,'--','src')}
for p in scripts.glob('*.py'):payload['runner/'+p.name]=p.read_bytes()
payload['runner/remote.py']=(root/'zkGPT-full12-work-20261029/remote.py').read_bytes()
payload['RESTORE.md']=b'''Verify LOCAL_VERIFICATION.json, EVIDENCE_CONTAINER.json and ARCHIVE_MANIFEST.json. evidence.tar.gz contains all386 frozen source/build files and public evidence. The large full12-v1.fsproof is stored once as a separate file; do not extract or duplicate it into the CAS. Verify its bytes and SHA independently. restore-cas contains deduplicated non-proof restore bytes; Linux modes and paths are in the manifest/tar.

Source bytes are authoritative; SOURCE_BASELINE_TO_FINAL.patch is supplemental. Build and execute only in the authorized fixed /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work directory. Do not overwrite the preserved original zkGPT repository or create sibling source trees. Windows authority is zkGPT-R7-dev. The enclosing remote parent Git repository is unrelated and must never be reset or staged by this task.

Restore MCL source from verified migration-20261020/archives/B3-R5A.tar.gz source/3rd/mcl. Restore original120 parameter bytes using PARAMETER_RECOVERY_VERIFIED.json and PARAMETER_IDENTITY.json; all corresponding existing Windows CAS bytes and the historical archive SHA were rechecked this task. Do not substitute checkpoint, numerical profile or Exp table. Build config and exact linked/cnpy bytes are in identity-bytes/ and dependency-bytes/; use matching Linux toolchain from BUILD_DEPENDENCY_IDENTITY.json. Archived CMake caches are provenance, not a portable build tree.

Restore evidence/full12-chunks.anchor to reports/R7-FULL12-UNRESTRICTED-20261029-v1/full12-chunks.anchor; its SHA must equal the hard Pin in source/src/paper_r7/full12_model_pin.hpp. Restore FULL12_STATEMENT.bin and reference public X/Y. The independent verifier loads its own fixed Anchor and independently supplied read-only public Statement FD. Restore binaries with executable mode or rebuild via cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DCNPY_INCLUDE_DIR=$PWD/deps/cnpy/include -DCNPY_LIBRARY=$PWD/deps/cnpy/lib/libcnpy.a and cmake --build build/release --target r7_fs_full12_prover r7_fs_full12_verifier -j4.

Exact native P/V and negative commands, process measurements, limits and environment are in evidence/full-P.json, full-V.json, negative-*.json and runner/full_run_remote.py. The Windows transport runner imports pathlib.Path, changes to the fixed directory and asserts it before executing the script. Use new exclusive output filenames for reruns; retain existing evidence. Never blindly rerun source-mutation implementation scripts from runner/; final source/ bytes are the deliverable. For Python execution directly on Linux, import pathlib.Path first and explicitly assert the fixed cwd. Full12 has no arbitrary wall timeout and no8GiB AS cap; inherit unlimited hard AS, four threads/allowed CPUs, MALLOC_ARENA_MAX=2. Resource guard only stops own task group under actual memory/disk pressure. Do not use sudo or change server/global/other-user settings.

The preserved Proof is a candidate full12 engineering acceptance with complete verifier-local coverage. Security status remains FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false. The small synthetic proof is component evidence only. Some inherited Oneblock P_ATTACK debug hooks are unsupported for Full12; use the actual statement/proof-message negative runner, which does not use those hooks.

Archive excludes private witness, blindings, SSH configuration, credentials, Git internals and historical large proofs. The private checkpoint remains only under remote build/private-full12 with0700/0600 permissions. No remote cleanup/deletion is authorized.
'''
data={n:base64.b64encode(b).decode() for n,b in payload.items()}
code="from pathlib import Path\nimport os,json,base64\nos.chdir('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')\nstage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1')\ndata=json.loads(%r)\nfor name,raw in data.items():\n p=stage/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(base64.b64decode(raw))\nprint('DELIVERY_METADATA_UPLOADED',len(data))\n"%json.dumps(data)
subprocess.run(['ssh','-F',str(root/'4090_sub2api_ssh_config'),'4090_sub2api','python3 -'],input=code.encode(),check=True)
print('SOURCE_COMMIT',head,flush=True)
