#!/usr/bin/python3
import sys,pathlib,time
root=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6-FULL12-P0AB-20261021/test-runs/serial-q1qjjolp')
with (root/"dispatch.log").open("a") as f:f.write('prover\n')
out=root/"output";out.mkdir()
time.sleep(5)
