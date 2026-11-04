"""Run one real R7 Single-Hyrax scope on Linux and retain raw evidence."""
import hashlib
import json
import os
from pathlib import Path
import resource
import shutil
import subprocess
import sys
import time

ROOT = Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
BASE = ROOT / 'reports/R7-SINGLE-HYRAX-20261103-v1/validation-20261104'
BIN = ROOT / 'build/release/r7-single-hyrax-20261104-v1/src/paper_r7'
EXPECTED = json.loads((BASE / 'AFTER_SOURCE_SHA256.json').read_text())
ENV = os.environ.copy()
ENV.update(ZKGPT_THREADS='4', MALLOC_ARENA_MAX='2', ZKGPT_OPT2_METRICS='1',
           ZKGPT_U_REFERENCE='0', ZKGPT_U_THREADS='4', ZKGPT_K_REFERENCE='0',
           ZKGPT_K_RAM_BYTES='536870912', ZKGPT_K_FILE_BYTES='4294967296')


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(4 << 20), b''):
            digest.update(chunk)
    return digest.hexdigest()


def save(path, value):
    Path(path).write_text(json.dumps(value, indent=2), encoding='utf-8')


def limits():
    soft, hard = resource.getrlimit(resource.RLIMIT_AS)
    if soft != hard:
        resource.setrlimit(resource.RLIMIT_AS, (hard, hard))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    os.sched_setaffinity(0, sorted(os.sched_getaffinity(0))[:4])


def run(stage, name, command, statement):
    handles = []
    env = ENV.copy()
    for key, path in (
        ('R7_ONEBLOCK_STATEMENT_FD', statement),
        ('R7_ONEBLOCK_COVERAGE_FD', stage / (name + '-COVERAGE.json')),
        ('R7_CLASSIFIED_LAYOUT_FD', stage / (name + '-LAYOUT.json')),
    ):
        stream = path.open('rb' if key.endswith('STATEMENT_FD') else 'xb')
        handles.append(stream)
        env[key] = str(stream.fileno())
    started = time.monotonic()
    with (stage / (name + '.log')).open('xb') as log:
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=log,
                                   stderr=subprocess.STDOUT, env=env,
                                   pass_fds=tuple(f.fileno() for f in handles),
                                   preexec_fn=limits)
        save(BASE / 'ACTIVE.json', {'phase': name, 'stage': str(stage),
                                    'pid': process.pid, 'command': command})
        _, status, usage = os.wait4(process.pid, 0)
        code = os.waitstatus_to_exitcode(status)
        process.returncode = code
    for stream in handles:
        stream.close()
    result = {'command': command, 'exit_code': code,
              'wall_seconds': time.monotonic() - started,
              'peak_rss_kib': usage.ru_maxrss,
              'user_cpu_seconds': usage.ru_utime,
              'system_cpu_seconds': usage.ru_stime}
    save(stage / (name + '.json'), result)
    print(json.dumps({'name': name, **result}), flush=True)
    return result


def tagged(path, prefix):
    out = []
    for line in Path(path).read_text(errors='replace').splitlines():
        if line.startswith(prefix + ' '):
            out.append(json.loads(line[len(prefix) + 1:]))
    return out


def rows(path):
    out = []
    for line in Path(path).read_text(errors='replace').splitlines():
        if line.startswith('{'):
            try:
                out.append(json.loads(line))
            except ValueError:
                pass
    return out


def current_identity():
    sources = {name: sha(ROOT / name) for name in EXPECTED
               if name.startswith('src/') or name in ('CMakeLists.txt', 'build.sh')}
    assert sources == EXPECTED, 'remote source SHA changed'
    parameters = {path.relative_to(ROOT).as_posix(): sha(path)
                  for path in (ROOT / 'data').rglob('*') if path.is_file()}
    reference = json.loads((ROOT / 'reports/R7-OPT3B-FULL12-20261030-v1/PARAMETER_SHA.json').read_text())
    assert parameters == reference and len(parameters) == 120, 'parameter identity changed'
    trust_paths = (
        'reports/R6A/oneblock-model-manifest-v1.txt',
        'reports/R6A/full12-model-manifest-v1.txt',
        'reports/R6-ROWMAJOR-v2/oneblock-operator-trust-rowmajor-v2.anchor',
        'reports/R7-FULL12-UNRESTRICTED-20261029-v1/full12-chunks.anchor',
    )
    trust = {name: sha(ROOT / name) for name in trust_paths}
    assert trust[trust_paths[-1]] == '02ed086e3a5b596e193ded8ede2e5de5da38a1358ff628068cea1e483e1e90c1'
    return {'sources': sources, 'parameters': parameters, 'trust': trust,
            'binaries': {p.name: sha(p) for p in BIN.iterdir()
                         if p.is_file() and os.access(p, os.X_OK)},
            'cmake_cache': sha(ROOT / 'build/release/r7-single-hyrax-20261104-v1/CMakeCache.txt')}


def execute(scope):
    assert scope in ('oneblock', 'full12')
    stage = BASE / ('single-hyrax-' + scope + '-v2')
    stage.mkdir(exist_ok=False)
    identity = current_identity()
    save(stage / 'IDENTITY_BEFORE.json', identity)
    statement_source = ROOT / 'reports/R7-OPT3C-K-FOLDING-20261102-v1' / (scope + '-v1/PROTOCOL1_STATEMENT.bin')
    expected_statement = {
        'oneblock': '46d050f75220d8f78e657e786ac8612ff25320e482136774ca2a1b9f6af8c37d',
        'full12': 'ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b',
    }[scope]
    assert sha(statement_source) == expected_statement, 'validated statement SHA mismatch'
    statement = stage / 'STATEMENT.bin'
    shutil.copyfile(statement_source, statement)
    assert sha(statement) == expected_statement
    save(stage / 'STATEMENT_REUSE.json', {'source': str(statement_source),
         'sha256': sha(statement), 'bytes': statement.stat().st_size,
         'fresh_real_witness_in_new_prover': True})
    meminfo = {line.split(':')[0]: int(line.split()[1]) for line in
               Path('/proc/meminfo').read_text().splitlines()
               if line.startswith(('MemAvailable:', 'SwapFree:'))}
    disk = shutil.disk_usage(ROOT)
    affinity = sorted(os.sched_getaffinity(0))[:4]
    assert meminfo['MemAvailable'] > 64 * 1024 * 1024
    assert disk.free > 32 << 30
    save(stage / 'ENVIRONMENT.json', {'env': {k: v for k, v in ENV.items()
         if k.startswith('ZKGPT_') or k == 'MALLOC_ARENA_MAX'}, 'memory_kib': meminfo,
         'disk_free_bytes': disk.free, 'affinity': affinity})
    proof = stage / (scope + '.fsproof')
    context = 'zkGPT/R7/single-hyrax/' + scope + '/20261104/v1'
    prover = BIN / ('r7_fs_' + scope + '_prover')
    verifier = BIN / ('r7_fs_' + scope + '_verifier')
    p = run(stage, 'P', [str(prover), 'prove', str(proof), context], statement)
    assert p['exit_code'] == 0 and proof.is_file(), 'prover failed'
    proof_sha = sha(proof)
    v = run(stage, 'V', [str(verifier), 'verify', str(proof), context], statement)
    assert v['exit_code'] == 0, 'independent verifier failed'
    coverage = json.loads((stage / 'V-COVERAGE.json').read_text())
    required = coverage['required']
    assert coverage['required_count'] == coverage['verified_count'] == len(required)
    assert all(entry['status'] == 'VERIFIED' for entry in required)
    ps = [entry for entry in rows(stage / 'P.log') if 'challenge_digest' in entry]
    vs = [entry for entry in rows(stage / 'V.log') if 'challenge_digest' in entry]
    assert len(ps) == len(vs) == 1
    for key in ('suite', 'session', 'proof_sha256', 'proof_bytes',
                'transcript_digest', 'challenge_digest', 'challenges',
                'prover_messages', 'events', 'complete'):
        assert ps[0][key] == vs[0][key], key
    assert ps[0]['complete'] and ps[0]['proof_sha256'] == proof_sha
    prover_census = [entry for entry in rows(stage / 'P.log') if 'root_count' in entry]
    verifier_census = [entry for entry in rows(stage / 'V.log') if 'root_count' in entry]
    assert len(prover_census) == len(verifier_census) == 1
    expected_suite = 'OneblockSingleHyrax' if scope == 'oneblock' else 'Full12SingleHyrax'
    assert ps[0]['suite'] == vs[0]['suite'] == expected_suite
    assert prover_census[0]['root_count'] == verifier_census[0]['root_count'] == 1
    assert prover_census[0]['status'].endswith('ACCEPT')
    assert identity == current_identity(), 'source/parameter/trust/binary changed during run'
    save(stage / 'IDENTITY_AFTER.json', identity)
    save(stage / 'RESULT.json', {'success': True, 'scope': scope,
         'suite': ps[0]['suite'], 'root_count': prover_census[0]['root_count'],
         'required_count': coverage['required_count'],
         'verified_count': coverage['verified_count'], 'proof_sha256': proof_sha,
         'proof_bytes': proof.stat().st_size, 'statement_sha256': sha(statement),
         'prover': p, 'verifier': v, 'prover_summary': prover_census[0],
         'verifier_summary': verifier_census[0], 'fs_prover': ps[0], 'fs_verifier': vs[0],
         'a3_timing_prover': tagged(stage / 'P.log', 'A3_TIMING'),
         'a3_timing_verifier': tagged(stage / 'V.log', 'A3_TIMING'),
         'security_status': 'FS_NIZK_CANDIDATE',
         'production_gate': 'BLOCKED_CRYPTOGRAPHIC_DESIGN', 'may_enter_stage2': False})
    print('SINGLE_HYRAX_SCOPE_COMPLETE ' + scope, flush=True)


if __name__ == '__main__':
    execute(sys.argv[1])
