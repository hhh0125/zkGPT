"""Independent Single Hyrax P/V fixtures and malformed-proof rejection tests."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile


def main():
    binary = Path(sys.argv[1]).resolve()
    output = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(tempfile.mkdtemp(prefix='r7-single-identity-'))
    output.mkdir(parents=True, exist_ok=True)
    records = []

    def run(name, args, success=True):
        command = [str(binary), *map(str, args)]
        p = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (output / (name + '.log')).write_text(p.stdout + p.stderr)
        records.append({'name': name, 'command': command, 'exit': p.returncode})
        assert (p.returncode == 0) == success, (name, p.stdout, p.stderr)
        return p

    vectors = run('identities-and-guards', ['guards'])
    expected = {
        1: '9741406a4c01bcb9d70204ac6ca38b9cb2747545dd9b9013b44efc36a866e0e0',
        4: '6820484fb82511bee0316d45395e7154ede57921dcefa4e4842347ef3a9c0957',
        128: 'b51f210edf2ee6692c33a065befcf0c8e85b1b94ba1b0eb29287e19054b6c8be',
        256: '40d06c975ef3615835b81b280f54049953d3912f3f25666c6abc9d6e099349f6',
        512: '2988d53b21757ca940206311cd2d1c2a0ef145b385d7b93ec941058bf06e929f',
    }
    actual = {}
    for line in vectors.stdout.splitlines():
        if line.startswith('VECTOR '):
            fields = dict(part.split('=', 1) for part in line.split()[1:])
            actual[int(fields['rows'])] = fields['identity']
    assert actual == expected, actual
    for mode in ['normal', 'lifecycle', 'row', 'plan', 'point', 'evaluation', 'role', 'session', 'old-identity']:
        proof = output / (mode + '.proof')
        p = run(mode + '-P', ['prove', mode, proof])
        v = run(mode + '-V', ['verify', mode, proof], mode in ['normal', 'lifecycle'])
        # Each independent process computes the full commitment identity exactly once,
        # although each successful proof contains two openings.
        assert p.stderr.count('SINGLE_HYRAX_IDENTITY ') == 1
        if mode in ['normal', 'lifecycle']:
            assert v.stderr.count('SINGLE_HYRAX_IDENTITY ') == 1
            pm, vm = json.loads(p.stdout), json.loads(v.stdout)
            for key in ['session', 'transcript_digest', 'challenge_digest', 'challenges', 'complete']:
                assert pm[key] == vm[key], key
            assert pm['complete']
        else:
            assert any(reason in v.stderr for reason in ['statement/parent binding mismatch', 'unexpected', 'identity mismatch']), v.stderr
    # A replacement commitment constructed from even identical bytes is unsealed;
    # the guards test covers that object lifecycle independently of wire rejection.
    proof = output / 'normal.proof'
    data = proof.read_bytes()
    suite = b'R7-ONEBLOCK-SINGLE-HYRAX-DIRECT-LOGICAL-FIVE-CATEGORIES-COMMITTED-X-FS-CANDIDATE-v6'
    assert data.count(suite) == 1
    old = output / 'old-suite.proof'
    old.write_bytes(data.replace(suite, suite[:-1] + b'5', 1))
    p = run('old-suite-V', ['verify', 'normal', old], False)
    assert 'no downgrade' in p.stderr
    (output / 'RESULT.json').write_text(json.dumps({'success': True, 'records': records}, indent=2) + '\n')
    print('PASS identity reference, immutable seal lifecycle, repeated openings, independent P/V and negatives:', output)


if __name__ == '__main__':
    main()
