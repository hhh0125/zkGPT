#!/usr/bin/python3
import sys,pathlib,time
root=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-OPT-20261021/test-runs/serial-7kv6s9mx')
with (root/"dispatch.log").open("a") as f:f.write('verifier\n')
print("SIMULATION ACCEPT");(root/"verified-cwd").write_text(str(pathlib.Path.cwd()))
