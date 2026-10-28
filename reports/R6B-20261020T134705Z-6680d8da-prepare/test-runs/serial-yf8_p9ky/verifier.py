#!/usr/bin/python3
import sys,pathlib,time
root=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-prepare/test-runs/serial-yf8_p9ky')
with (root/"dispatch.log").open("a") as f:f.write('verifier\n')
print("SIMULATION ACCEPT");(root/"verified-cwd").write_text(str(pathlib.Path.cwd()))
