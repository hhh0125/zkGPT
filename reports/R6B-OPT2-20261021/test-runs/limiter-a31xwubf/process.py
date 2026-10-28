import subprocess,sys,time,signal
p=subprocess.Popen([sys.executable,"-c","import signal,time;signal.signal(signal.SIGTERM,signal.SIG_IGN);time.sleep(60)"])
open('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-OPT2-20261021/test-runs/limiter-a31xwubf/grandchild.pid',"w").write(str(p.pid))
time.sleep(60)
