from pathlib import Path
import json,traceback
exec((Path(__file__).with_name('run_stage_retained.py')).read_text().split('mode=sys.argv[1]')[0])
def limits(test=True,full_ctest=False):
 os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4]);resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,(4<<30,)*2);resource.setrlimit(resource.RLIMIT_CPU,(120,)*2)
try:
 assert json.loads((BASE/'retained-SUCCESS.json').read_text())['success']
 before=identity();save('retained-verify-IDENTITY_BEFORE.json',before)
 exe=BUILD/'src/paper_r7/r7_perf2_protocol';proof=BASE/'bounded-after.proof'
 assert 'ZKGPT_PERF2_K_REFERENCE' not in ENV
 run('retained-independent-EOF',[exe,'verify',proof,'524288'],60)
 for label in ['truncated','appended','mutated']:run('retained-reject-'+label,[exe,'verify',BASE/('negative-'+label+'.proof'),'524288'],60,True,1)
 after=identity();save('retained-verify-IDENTITY_AFTER.json',after);assert before==after,'STOP_SYNC inputs changed'
 save('retained-verify-SUCCESS.json',{'success':True,'performance_experiments':False,'purpose':'final default-off delivery correctness only'})
except BaseException as e:save('retained-verify-FAILURE.json',{'error':str(e),'traceback':traceback.format_exc()});raise
