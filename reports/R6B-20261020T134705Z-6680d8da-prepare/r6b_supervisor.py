"""Reuse frozen run_limited/helper protection; add unconditional group cleanup.

Only namespace adapters differ: output paths, environment, affinity and finally.
The archived helper source is never edited. No model is launched by importing.
"""
import subprocess,os,signal,time,shutil,types,io,contextlib
from pathlib import Path
from r6b_common import *

class LimitedSupervisor:
    def __init__(self,directory,helper,helper_sha,disk_usage=shutil.disk_usage):
        self.directory=Path(directory);self.helper=Path(helper);self.helper_sha=helper_sha;self.disk_usage=disk_usage
    def run(self,job):
        require(type(job['memory_gib']) is int and 1<=job['memory_gib']<=256,'supervisor memory cap')
        require(type(job['seconds']) is int and 1<=job['seconds']<=86400,'supervisor wall cap')
        require(job['cpu_limit']==4 and job['expected_exit']==0 and 8<=job['disk_reserve_gib']<=8192,'supervisor CPU/disk/exit contract')
        import re
        require(re.fullmatch('[A-Za-z0-9][A-Za-z0-9_.-]{0,80}',job['id']) is not None,'job id')
        require(all(isinstance(x,str) for x in job['argv']) and Path(job['argv'][0]).is_absolute(),'absolute executable/structured argv')
        require(file_sha(self.helper)==self.helper_sha,'reused limiter identity changed')
        output=self.directory/'supervisor';output.mkdir(exist_ok=True)
        for suffix in ['.log','.time','.json','-supervision.json']:
            require(not (output/(job['id']+suffix)).exists(),'supervisor output collision')
        cwd=Path(job['cwd']);require(cwd.is_absolute() and not cwd.is_symlink(),'invalid cwd')
        if job['fresh_empty_cwd']:
            require(not cwd.exists(),'independent verifier cwd already exists');cwd.mkdir(parents=False);require(not any(cwd.iterdir()),'independent cwd nonempty')
        else:require(cwd.is_dir(),'working directory missing')
        groups=[];env=child_environment();native_popen=subprocess.Popen
        def tracked_popen(*args,**kwargs):
            require(kwargs.get('start_new_session') is True,'limiter must create process group')
            require(kwargs.get('env')==env,'limiter environment differs from approved whitelist')
            prior=kwargs['preexec_fn']
            def limits():
                prior();cpus=sorted(os.sched_getaffinity(0))[:4];require(len(cpus)==4,'four CPUs unavailable');os.sched_setaffinity(0,cpus)
            kwargs['preexec_fn']=limits
            p=native_popen(*args,**kwargs);groups.append(p);return p
        ns={'__file__':str(self.helper)};exec(compile(read_regular(self.helper),str(self.helper),'exec'),ns)
        ns['r']=output;ns['logs']=output
        ns['subprocess']=types.SimpleNamespace(Popen=tracked_popen,TimeoutExpired=subprocess.TimeoutExpired,STDOUT=subprocess.STDOUT)
        ns['os']=types.SimpleNamespace(environ=env,killpg=os.killpg)
        ns['shutil']=types.SimpleNamespace(disk_usage=self.disk_usage)
        failure=None;captured=io.StringIO();termination=[]
        prior_handlers={sig:signal.getsignal(sig) for sig in [signal.SIGINT,signal.SIGTERM]}
        def defer_termination(signum,frame):termination.append(signum)
        for sig in prior_handlers:signal.signal(sig,defer_termination)
        original_disk=self.disk_usage
        def checked_disk(path):
            if termination:raise GateError('termination requested')
            return original_disk(path)
        ns['shutil']=types.SimpleNamespace(disk_usage=checked_disk)
        try:
            with contextlib.redirect_stdout(captured):
                ns['run'](job['id'],job['argv'],expected=0,timeout=job['seconds'],mem_gib=job['memory_gib'],cwd=cwd,disk_gib=job['disk_reserve_gib'],reserve_gib=32)
        except BaseException as error:failure=type(error).__name__+': '+str(error)
        finally:
            # Always kill the group even if /usr/bin/time exited first while a
            # grandchild ignored TERM. A monitor exception cannot orphan work.
            for p in groups:
                for sig in [signal.SIGTERM,signal.SIGKILL]:
                    try:os.killpg(p.pid,sig)
                    except ProcessLookupError:pass
                    if sig==signal.SIGTERM:time.sleep(.05)
                try:p.wait(timeout=2)
                except subprocess.TimeoutExpired:pass
            for sig,handler in prior_handlers.items():signal.signal(sig,handler)
        result=output/(job['id']+'.json')
        row=json_read(result) if result.exists() else {'exit_code':None,'stop_reason':'supervisor_exception','log':str(output/(job['id']+'.log'))}
        if failure and row.get('exit_code')==0:row['stop_reason']='supervisor_exception'
        if termination:row['stop_reason']='termination_requested';failure=failure or 'termination requested'
        row.update({'environment':env,'limiter_sha256':self.helper_sha,'supervisor_exception':failure,'deferred_termination_signals':termination,'tracked_groups':[p.pid for p in groups],'finally_cleanup':True,'driver_output':captured.getvalue(),'cpu_affinity_limit':4})
        write_new(output/(job['id']+'-supervision.json'),row);return row
