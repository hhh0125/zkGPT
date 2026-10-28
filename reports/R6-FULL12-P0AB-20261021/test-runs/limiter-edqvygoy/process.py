import subprocess,sys,time,signal
p=subprocess.Popen([sys.executable,"-c","import signal,time;signal.signal(signal.SIGTERM,signal.SIG_IGN);time.sleep(60)"])
open('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6-FULL12-P0AB-20261021/test-runs/limiter-edqvygoy/grandchild.pid',"w").write(str(p.pid))
time.sleep(60)
