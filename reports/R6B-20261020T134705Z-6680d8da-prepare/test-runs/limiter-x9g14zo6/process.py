import subprocess,sys,time,signal
p=subprocess.Popen([sys.executable,"-c","import signal,time;signal.signal(signal.SIGTERM,signal.SIG_IGN);time.sleep(60)"])
open('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-prepare/test-runs/limiter-x9g14zo6/grandchild.pid',"w").write(str(p.pid))
time.sleep(60)
