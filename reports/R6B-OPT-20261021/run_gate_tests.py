"""Entire lightweight gate suite. Contains no real model invocation."""
from pathlib import Path
import unittest,sys,json,datetime,time
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE))
class Results(unittest.TextTestResult):
    def __init__(self,*args,**kwargs):super().__init__(*args,**kwargs);self.records=[];self.subcases=[]
    def addSuccess(self,test):super().addSuccess(test);self.records.append({'test':test.id(),'status':'PASS'})
    def addFailure(self,test,err):super().addFailure(test,err);self.records.append({'test':test.id(),'status':'FAIL'})
    def addError(self,test,err):super().addError(test,err);self.records.append({'test':test.id(),'status':'ERROR'})
    def addSubTest(self,test,subtest,err):super().addSubTest(test,subtest,err);self.subcases.append({'test':str(subtest),'status':'PASS' if err is None else 'FAIL'})
if __name__=='__main__':
    stamp=sys.argv[1];log=HERE/(stamp+'.log');data=HERE/(stamp+'.json');assert not log.exists() and not data.exists()
    start=time.monotonic()
    with log.open('x') as stream:r=unittest.TextTestRunner(stream=stream,verbosity=2,resultclass=Results).run(unittest.defaultTestLoader.discover(str(HERE),pattern='test_*.py'))
    obj={'success':r.wasSuccessful(),'tests_run':r.testsRun,'seconds':time.monotonic()-start,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'cases':r.records,'subcases':r.subcases,'failures':len(r.failures),'errors':len(r.errors),'full12_witness_executed':False,'full12_proof_generated':False,'full12_independent_verifier_executed':False,'valid_full12_permits_generated':False,'simulation_success_is_not_crypto_acceptance':True}
    data.write_text(json.dumps(obj,indent=2));print(json.dumps({k:v for k,v in obj.items() if k not in ['cases','subcases']},indent=2));sys.exit(0 if r.wasSuccessful() else 1)
