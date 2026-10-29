"""Transport task scripts to the sole authorized workspace; never contains credentials."""
from pathlib import Path
import subprocess, sys
ROOT = Path(__file__).resolve().parent.parent
REMOTE = '/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work'
script = Path(sys.argv[1]).read_bytes()
prefix = ("import os\nfrom pathlib import Path\nos.chdir(" + repr(REMOTE) + ")\nassert Path.cwd().resolve() == Path(" + repr(REMOTE) + ")\n").encode()
result = subprocess.run(['ssh', '-F', str(ROOT/'4090_sub2api_ssh_config'), '4090_sub2api', 'python3 -'], input=prefix+script)
sys.exit(result.returncode)
