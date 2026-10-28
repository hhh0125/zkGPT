#!/usr/bin/python3
import sys,pathlib,time
root=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-OPT2-20261021/test-runs/serial-3se7esy4')
with (root/"dispatch.log").open("a") as f:f.write('verifier\n')
print("SIMULATION ACCEPT");(root/"verified-cwd").write_text(str(pathlib.Path.cwd()))
