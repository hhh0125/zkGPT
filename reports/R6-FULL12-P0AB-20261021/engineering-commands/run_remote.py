from pathlib import Path
import subprocess, sys
base=Path(__file__).resolve().parent
script=Path(sys.argv[1])
p=subprocess.run(['ssh','-F','4090_sub2api_ssh_config','4090_sub2api','cd /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work && python3 -'],input=script.read_bytes(),stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(base/(script.stem+'.remote.log')).write_bytes(p.stdout)
sys.stdout.buffer.write(p.stdout)
sys.exit(p.returncode)
