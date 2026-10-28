#!/usr/bin/python3
import sys,pathlib,time
root=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6-FULL12-P0AB-20261021/test-runs/serial-tzcjj3r5')
with (root/"dispatch.log").open("a") as f:f.write('verifier\n')
print("SIMULATION ACCEPT");(root/"verified-cwd").write_text(str(pathlib.Path.cwd()))
