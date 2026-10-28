"""Trusted native decoding, followed by independent complete public-layout admission."""
from pathlib import Path
import subprocess,resource,json
from r6b_common import *
MODULUS=16798108731015832284940804142231733909759579603404752749028378864165570215949
class Reader:
    def __init__(self,b):self.b=b;self.at=0
    def raw(self,n):
        require(type(n) is int and 0<=n<=len(self.b)-self.at,'truncated/oversized wire field');v=self.b[self.at:self.at+n];self.at+=n;return v
    def u(self):return int.from_bytes(self.raw(8),'little')
    def blob(self):return self.raw(self.u())
    def text(self):return self.blob().decode('ascii')
    def fr(self):
        v=int.from_bytes(self.raw(32),'little');require(v<MODULUS,'noncanonical Fr');return v
    def pc(self):
        spec=[self.text()]+[self.u() for _ in range(5)];n=self.u();require(n==spec[3],'PC rows');return spec,digest(self.raw(n*32))
    def done(self):require(self.at==len(self.b),'trailing wire bytes')
def decode_native(parser,statement,anchor=None,proof=None):
    argv=[str(parser),'--admit' if anchor else '--decode',str(statement)]+([str(anchor)] if anchor else [])+([str(proof)] if proof else [])
    def cap():resource.setrlimit(resource.RLIMIT_AS,(2<<30,2<<30));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    p=subprocess.run(argv,env=child_environment(),capture_output=True,text=True,timeout=600,preexec_fn=cap)
    require(p.returncode==0,'trusted artifact decoder: '+p.stderr[:512]);v=json.loads(p.stdout);require(v['decoded'] and not v['cryptographic_verified'],'decoder result');return v
def schema(plan):
    constants=set();zero=[]
    for region in plan['ranges']:
        n=region['name'];reserve=20 if n.startswith('round_') and n.endswith('_q') else 10 if ((n.startswith(('layer_norm_','gelu_')) and n.endswith('_y')) or (n.startswith('softmax_') and n.endswith('_sumE'))) else 0
        if reserve:at=region['offset']-reserve;constants.add(at);zero.extend(range(at+1,at+reserve))
    require(len(constants)==96,'full12 constant coverage');return sorted(constants),zero
def inspect_statement(statement,parser,anchor,plan_file,proof=None):
    statement=Path(statement);b=read_regular(statement,16<<20);before=digest(b)
    native=decode_native(parser,statement,anchor,proof);require(file_sha(statement,16<<20)==before,'statement replacement during decode')
    plan=json_read(plan_file);r=Reader(b);require(r.text()=='ZKGPTR2' and r.u()==3 and r.text()=='statement','R2 header');require(r.u()==2,'GKR protocol')
    require([r.u() for _ in range(6)]==[32,12,12,64,768,3072],'full12 shape');require(r.u()==28 and r.text()=='zkGPT/main' and r.u()==16384,'master GKR dimensions')
    require(r.raw(32).hex()==plan['circuit'] and r.fr()==0,'circuit/output identity');integration=r.raw(32).hex();require(integration!='00'*32,'zero integration context')
    require(r.u()==16384,'GKR PC rows');r.raw(16384*32);lookup=r.blob();r.done();s=Reader(lookup)
    require(s.text()=='ZKGPTR1' and s.u()==1 and s.u()==12,'lookup header')
    anchor_lines=read_regular(anchor,2<<20).decode().splitlines();context='R6-R5B-engineering-candidate-NOT-PAPER-APPROVED/'+anchor_lines[5]+'/'+plan['circuit']
    require(anchor_lines[1]=='r5b-engineering-candidate-v1' and anchor_lines[2]==plan['profile_sha256'] and anchor_lines[4]==plan['circuit'],'anchor Profile/circuit')
    require(s.text()==context,'model/Profile/context mismatch')
    master,_=s.pc();require(master==['r2/model/master',plan['master_actual'],plan['master_padded'],16384,16384,1],'master layout')
    require(s.u()==len(plan['children']),'children coverage')
    for child in plan['children']:
        pc,pcsha=s.pc();require(pc==[child['role'],child['actual'],child['padded'],child['rows'],child['columns'],0],'child layout')
        spans=child['spans'];require(s.u()==len(spans),'child source span count')
        for span in spans:require([s.u(),s.u(),s.raw(32).hex()]==[span['offset'],span['count'],span['bias']],'child master coordinates/bias')
        if child['role']=='r2/model/W':require(pcsha==digest(b''.join(bytes.fromhex(x) for x in anchor_lines[8:])),'full W anchor row identity')
    constants,zero=schema(plan);require(s.u()==65536+len(constants)+len(zero),'complete public coordinates count')
    x=[]
    for at in range(32768):
        require(s.u()==at,'public X coordinate');v=s.fr();v=v if v<=MODULUS//2 else v-MODULUS;x.append(v)
    xsha=validate_public_x(x);output=plan['connections'][-1]['output']
    for at in range(32768):
        require(s.u()==output+at,'public Y coordinate');v=s.fr();signed=v if v<=MODULUS//2 else v-MODULUS
        require(-(1<<63)<=signed<(1<<63),'Y signed64 domain');require(at%1024<768 or v==0,'Y padding')
    for at in constants:require(s.u()==at and s.fr()==1,'numeric public constant')
    for at in zero:require(s.u()==at and s.fr()==0,'reserved zero padding')
    ranges=[(i,c['role']) for i,c in enumerate(plan['children']) if c['role'].startswith('r2/model/range/')]
    # Derive each group's width from its role/order and the registered numeric spans;
    # registry grouping is ordered by (bits,signed), with duplicate widths allowed.
    groups=sorted(set((q['bits'],q['signed']) for q in plan['ranges']))
    require(len(groups)==len(ranges) and s.u()==len(ranges),'Range coverage')
    for (index,role),(bits,_) in zip(ranges,groups):require([s.u(),s.text(),s.u()]==[index,role,bits],'Range identity/width')
    ti=next(j for j,c in enumerate(plan['children']) if c['role']=='r2/model/Exp-t');ei=ti+1
    require(s.u()==1 and [s.u(),s.u(),s.text(),s.u()]==[ti,ei,'r2/model/Exp',1],'Exp identity/profile');s.done()
    return {'X_sha256':xsha,'statement_sha256':before,'statement_bytes':len(b),'anchor_W_equal':True,'shape':[32,12,12,64,768,3072],'context':context,'integration_context':integration,'integration_context_crypto_check':'deferred to actual independent verifyR2','proof_decoded':native['proof_decoded'],'cryptographic_verified':False}
def artifact_admission(policy):
    statement=Path(policy['output'])/'model.statement';proof=Path(policy['output'])/'model.proof'
    require(statement.stat().st_size>0 and proof.stat().st_size>0,'empty artifacts')
    hashes={str(p):{'sha256':file_sha(p,64<<20),'bytes':p.stat().st_size} for p in [statement,proof]}
    result=inspect_statement(statement,policy['parser'],policy['anchor'],policy['plan'],proof)
    require(all(file_sha(p,64<<20)==row['sha256'] for p,row in hashes.items()),'artifact changed during admission')
    return {'files':hashes,'statement':result,'cryptographic_verified':False}
