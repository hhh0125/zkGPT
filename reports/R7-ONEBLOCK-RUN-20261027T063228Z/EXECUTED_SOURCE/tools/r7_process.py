import subprocess,os,signal,time
def start_interactive_child(command,**kwargs):
 """Central launch policy, regression-tested with the outer timeout wrapper."""
 # P/V remain different processes/address spaces, inside the outer run's group.
 return subprocess.Popen(command,start_new_session=False,**kwargs)
def run_process_group(command,log,timeout,env=None,preexec_fn=None):
 """Linux execution: a timed-out build/prover cannot leave descendants running."""
 process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,env=env,preexec_fn=preexec_fn,start_new_session=True)
 def stop_group():
  try:os.killpg(process.pid,signal.SIGTERM)
  except ProcessLookupError:return
  time.sleep(.2)
  try:os.killpg(process.pid,signal.SIGKILL)
  except ProcessLookupError:pass
 try:
  try:code=process.wait(timeout=timeout)
  except subprocess.TimeoutExpired:stop_group();process.wait();return 124
  # These commands must not leave background services even on normal exit.
  stop_group();return code
 except BaseException:
  stop_group();process.wait();raise
