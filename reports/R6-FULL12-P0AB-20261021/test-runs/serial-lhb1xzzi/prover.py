#!/usr/bin/python3
import sys,pathlib,time
root=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6-FULL12-P0AB-20261021/test-runs/serial-lhb1xzzi')
with (root/"dispatch.log").open("a") as f:f.write('prover\n')
out=root/"output";out.mkdir()
(out/"model.statement").write_text('SIMULATION STATEMENT')
(out/"model.proof").write_text('SIMULATION PROOF')
