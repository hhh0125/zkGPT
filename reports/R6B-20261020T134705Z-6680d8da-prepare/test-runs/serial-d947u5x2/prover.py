#!/usr/bin/python3
import sys,pathlib,time
root=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-prepare/test-runs/serial-d947u5x2')
with (root/"dispatch.log").open("a") as f:f.write('prover\n')
out=root/"output";out.mkdir()
(out/"model.statement").write_text('SIMULATION STATEMENT')
