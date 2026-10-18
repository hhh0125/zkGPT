"""Independent full12 checker: scalar rules + raw PRE-subset gate accesses.

This process never calls buildExpectedPlan. Its trusted inputs are the pinned
deployment, approved source tree and the public builder it launches itself.
gate-origins.bin is local instrumented execution evidence, never proof input.
All normalized spans are compared against complete, independently deduplicated
raw streams. No mapping digest is used as a coverage test.
"""
import argparse
import array
import hashlib
import json
import math
import pathlib
import resource
import struct
import subprocess
import sys
import time

MODEL = 'd3c62c91c331e828c1141440ac1822b3da3e26c3ba7d8c28cf4c42e50a127c65'
PROFILE = '3965ecaa02a1bd52181cf3d5e6ec40ffb5b785e9ac3b51b876d15dd03d779ca8'
ANCHOR = '464e722650b16c5205d6f0576a1059571a6160e032bd8ad30b4d618a22b31de3'

def need(ok, message):
    if not ok:
        raise ValueError(message)

def same(actual, expected, label):
    if type(actual) is not type(expected):
        raise ValueError(f'{label}: type {type(actual).__name__} instead of {type(expected).__name__}')
    if actual is expected:
        return
    # Native equality finds changed values quickly, but Python considers bool,
    # int and float values equal in nested containers. Check every nested type
    # as well, without formatting a multi-GB manifest into an error message.
    if actual != expected:
        raise ValueError(f'{label}: values differ')
    if isinstance(expected,dict):
        for key in expected:
            same(actual[key],expected[key],label+'/'+key)
    elif isinstance(expected,list):
        for index,(a,b) in enumerate(zip(actual,expected)):
            same(a,b,label+'/'+str(index))

def padded(n):
    need(n > 0, 'empty length')
    return 1 << (n - 1).bit_length()

def scalar_layout():
    """Append arithmetic independently transcribed from reviewed builder rules."""
    parameters, constants, regions, exp, fc_ports = [], [], [], [], {}
    pos = 32 * 1024
    for f in range(48):
        ci, co, ri, ro = [(1024,4096,768,2304),(1024,1024,768,768),
                          (1024,4096,768,3072),(4096,1024,3072,768)][f % 4]
        parameters.append([f'fc_{f}.weight', pos, ci*co, ro, ri])
        pos += ci*co
        parameters.append([f'fc_{f}.bias', pos, co, ro, 1])
        pos += co

    def region(kind, prefix, instance, name, off, count, bits, sign=False):
        regions.append([kind, f'{prefix}_{instance}_{name}', off, count, bits, sign])

    def ln(instance):
        nonlocal pos
        for name, off in [('weight',pos), ('bias',pos+1024)]:
            parameters.append([f'ln_{instance}.{name}',off,1024,768,1])
        pos += 2048
        constants.append(pos)
        start = pos
        fields = [('y',10,32768,64,True), ('sum',32778,32,64,True),
                  ('B',32810,32,64,False), ('sigma',32842,32,64,False),
                  ('delta1',32874,32,64,False), ('delta2',32906,24576,126,False),
                  ('term1',65674,24576,64,False), ('term2',90250,24576,64,False)]
        for name, off, count, bits, sign in fields:
            region(1, 'layer_norm', instance, name, start+off, count, bits, sign)
        pos += 114826
        return start + 10

    def rounding(instance, count):
        nonlocal pos
        constants.append(pos)
        for j, (name,bits,sign) in enumerate([('q',64,True), ('delta',126,False),
                                             ('term1',64,False), ('term2',64,False)]):
            region(0, 'round', instance, name, pos+20+j*count, count, bits, sign)
        start = pos + 20
        pos += 20 + 4 * count
        return start

    for b in range(12):
        x = ln(2*b)
        weight_base = 32768 + 13641728*b
        fc_ports[23*b+4] = [(x,32768), (weight_base,4194304)]
        rounding(4*b,131072)
        constants.append(pos)
        o = pos
        # H=384, causal triangle T=6336, head hidden J=24576.
        fields = [('sumE',10,384,32), ('pmax',394,384,64),
                  ('delta1',778,6336,64), ('delta2',7114,6336,126),
                  ('t',13450,6336,32), ('E',19786,6336,32),
                  ('delta3',26122,24576,126), ('output',50698,32768,64),
                  ('lookup_term1',83466,6336,64), ('lookup_term2',89802,6336,64),
                  ('division_term1',96138,24576,64), ('division_term2',120714,24576,64)]
        for name, off, count, bits in fields:
            region(3,'softmax',b,name,o+off,count,bits,name=='output')
        exp += [[b,j,o+13450+j,o+19786+j] for j in range(6336)]
        pos += 145290
        # FC1 uses softmax final output, whose channel width is 1024 here.
        fc_ports[23*b+10] = [(o+50698,32768), (weight_base+4198400,1048576)]
        rounding(4*b+1,32768)
        pos += 32768
        x = ln(2*b+1)
        fc_ports[23*b+16] = [(x,32768), (weight_base+5248000,4194304)]
        rounding(4*b+2,131072)
        constants.append(pos)
        o = pos
        fields = [('y',10,131072,64,True), ('abs',131082,131072,64,False),
                  ('t',262154,131072,1,False), ('delta1',393226,98304,64,False),
                  ('delta2',491530,98304,64,False), ('delta3',589834,98304,126,False),
                  ('term1',688138,98304,64,False), ('term2',786442,98304,64,False)]
        for name, off, count, bits, sign in fields:
            region(2,'gelu',b,name,o+off,count,bits,sign)
        pos += 884746
        fc_ports[23*b+21] = [(o+10,131072), (weight_base+9446400,4194304)]
        rounding(4*b+3,32768)
        output = pos
        pos += 32768

    # Independent insertion-order grouping, checking every complete region.
    groups, current = [], {}
    for r in regions:
        bits, count = r[4], r[3]
        if bits not in current or groups[current[bits]][1] + count > 2**22:
            current[bits] = len(groups)
            groups.append([bits,0,0])
        q = current[bits]
        r += [q,groups[q][1]]
        groups[q][1] += count
    for g in groups:
        g[2] = padded(g[1])
    return dict(n0=pos,N0=padded(pos),ell=(padded(pos)).bit_length()-1,
                output_offset=output,constants=constants,parameters=parameters,
                regions=regions,groups=groups,exp=exp), fc_ports

def compress(values):
    out = []
    for x in values:
        if out and out[-1][0]+out[-1][1] == x:
            out[-1][1] += 1
        else:
            out.append([x,1,0])
    return out

def verify_raw(path, layout, fc_ports):
    """Read all original accesses; independently dedup in first-seen order."""
    total_accesses = total_mapped = 0
    with path.open('rb') as f:
        def u64():
            b = f.read(8)
            need(len(b)==8, 'truncated raw gate stream')
            return struct.unpack('<Q',b)[0]
        same(u64(),0x31304749524f4252,'origin magic')
        same(u64(),276,'origin layer count')
        for i, layer in enumerate(layout['layers'],1):
            same([u64(),u64(),u64(),u64()],
                 [i,layer['type'],layer['size'],layer['zero_start']],f'raw layer {i}')
            for port in range(2):
                n = u64()
                need(n <= 2**29, 'raw gate stream budget')
                raw = array.array('I')
                need(raw.itemsize==4, 'requires 32-bit unsigned array')
                raw.fromfile(f,n)
                if sys.byteorder != 'little':
                    raw.byteswap()
                total_accesses += n
                if i in fc_ports:
                    start,count = fc_ports[i][port]
                    same(n,count,f'FCONN {i}/{port} count')
                    need(raw == array.array('I',range(start,start+count)),f'FCONN {i}/{port} row-major mapping')
                    expected = [[start,count,0]]
                    m = count
                else:
                    # dict insertion ordering is defined in Python >=3.7.
                    # Derivation consumes EVERY original gate operand; it does
                    # not invert the already remapped gates using ori_id.
                    unique = array.array('I',dict.fromkeys(raw))
                    m = len(unique)
                    need(not m or max(unique)<layout['n0'],f'port {i}/{port} bounds')
                    expected = compress(unique)
                    del unique
                same(layer['projection'][port],expected,f'complete mapping {i}/{port}')
                same(layer['m'][port],m,f'actual length {i}/{port}')
                same(layer['d'][port],(m-1).bit_length() if m else -1,f'padding dimension {i}/{port}')
                total_mapped += m
        need(f.read(1)==b'', 'extra raw origin records')
    return total_accesses,total_mapped

def pc(scheme,domain,oracle,relation,rows,columns):
    return dict(scheme=scheme,domain=domain,oracle=oracle,relation=relation,rows=rows,columns=columns)

def main_pc():
    return pc('Hyrax-row-IPA-Fr-BN254','zkGPT/main','C0','mixed-input;row-major;lsb-first',16384,16384)

def range_pc(q,p):
    return pc('Hyrax-row-IPA-Fr-BN254','zkGPT/range',f'range/Z/{q}',
              'linear-combination-of-same-D-chunk-rows',p//4096,4096)

def exp_pc(k):
    return pc('Hyrax-row-IPA-Fr-BN254','zkGPT/exp-lookup/v4',f'exp/vector/{k}',
              'same-parent-exp-eight-vector-rows',256,512)

def direct_pc(k):
    return pc('vector-IPA-Fr-BN254','zkGPT.route-b.lasso-prototype.v1.vector-ipa',f'direct/Q/{k}',
              'same-Q-as-local-B1-and-opening;D1-to-C0-required',1,256)

def source(identity,kind,m,projection,commitment,dependencies,**kw):
    s = dict(id=identity,kind=kind,m=m,p=padded(m),d=(m-1).bit_length(),support=m,
             shape=[m],projection=projection,bits=0,is_signed=False,
             alias='unique-within-source;reuse-across-sources',padding='0',
             value_rule='field',pc=commitment,dependencies=dependencies)
    s.update(kw)
    return s

def obligation(identity,kind,**kw):
    o = dict(id=identity,kind=kind,relation='',table='',index=0,subindex=0,m=0,p=0,
             shift=0,bits=0,pc=pc('','','','',0,0),dim_pc=pc('','','','',0,0),
             child_id='',native_padding='',query_length=0,table_size=0,dependencies=[])
    o.update(kw)
    return o

def expected_plan(mode, l, table_id):
    sources, obs = [], []
    for layer in l['layers']:
        i = layer['index']
        fc = (i-1)%23+1 in [4,10,16,21]
        for point in range(2 if fc else 1):
            for port in range(2):
                if layer['m'][port]:
                    sources.append(source([0,i,port,point],'gkr-input',layer['m'][port],
                                          layer['projection'][port],main_pc(),[f'gkr/layer/{i}']))
        fields = dict(index=i,m=layer['size'],p=padded(layer['size']),shift=layer['zero_start'],pc=main_pc())
        obs.append(obligation(f'gkr/layer/{i}','gkr-layer',**fields,
                    relation='two-matrix-points-full-contraction-and-output-coordinates;continuation-is-not-input' if fc else 'all-unary-binary-gates;continuation-to-previous-layer',dependencies=[f'gkr/zero/{i}']))
        obs.append(obligation(f'gkr/zero/{i}','gkr-zero',**fields,
                    relation='V[i][zero_start:size]=0;last-check-output-is-zero-not-public-Y'))
    for q,(bits,m,p) in enumerate(l['groups']):
        projection=[]
        for r in l['regions']:
            if r[6]==q:
                same(r[7],sum(x[1] for x in projection),'Range group placement')
                projection.append([r[2],r[3],r[4] if r[5] else 0])
        sources.append(source([1,q,0,0],'range-Z',m,projection,range_pc(q,p),[f'range/reconstruction/{q}'],bits=bits,
                              value_rule='w[pi]+per-region-signed-bias;Z=sum(2^shift*D)'))
        deps=[]
        for h,shift in enumerate(range(0,bits,9)):
            identity=f'range/chunk/{q}/{h}'
            deps.append(identity)
            commit=range_pc(q,p)
            commit.update(oracle=f'range/D/{q}/{h}',relation='exact-same-D-as-reconstruction-and-lookup')
            obs.append(obligation(identity,'range-chunk',index=q,subindex=h,m=m,p=p,shift=shift,
                bits=min(9,bits-shift),table='integer-interval-[0,2^bits)',pc=commit,
                query_length=p,table_size=2**min(9,bits-shift),
                child_id=f'{"lasso" if mode==2 else "logup"}/range/{q}/{h}',
                native_padding='none;parent-tail-is-member-row' if mode==2 else 'legacy-parent-padding',
                relation='all-LogUp-membership-relations' if mode==1 else 'all-production-Lasso-membership-relations'))
        obs.append(obligation(f'range/reconstruction/{q}','range-reconstruction',index=q,m=m,p=p,bits=bits,
                    pc=range_pc(q,p),relation='same-D-row-linear-Z;encoded-bias;all-actual-and-padding-rows',dependencies=deps))
    for k in range(2):
        projection=compress([e[2+k] for e in l['exp']])
        sources.append(source([2,k,0,0],['exp-t','exp-E'][k],len(l['exp']),projection,exp_pc(k),
                ['exp/branch/0','exp/branch/1','range/uint32-t-E']+[f'exp/membership/{i}' for i in range(6)],bits=32,
                padding='1048576' if k else '0',value_rule='uint32;operator-major;query-minor;E-index=t-index+6336'))
    for i in range(2):
        obs.append(obligation(f'exp/branch/{i}','exp-branch',index=i,m=76032,p=131072,query_length=131072,pc=exp_pc(0),
            relation=['s*(t-K)=x-K','s*(2*K-1-2*t)=sum(256^h*d_h)-t+K'][i],
            table='K=6891;padding(t,E,s,x,d)=(0,1048576,1,0,6890);limbs=(234,26,0,0)',
            dependencies=['exp/eight-vector-openings','range/uint32-t-E']+[f'exp/membership/{j}' for j in range(6)]))
    for i in range(6):
        commit=exp_pc((1 if mode==2 else 3) if i==0 else 2 if i==1 else i+2)
        if i==0 and mode==1:
            commit['oracle']='exp/P=Cx+alpha*CE'
        obs.append(obligation(f'exp/membership/{i}','exp-membership',index=i,m=76032,p=131072,
            query_length=131072,table_size=8192 if i==0 else 2 if i==1 else 256,
            child_id=('lasso/exp/E-indexed' if i==0 else 'lasso/exp/s' if i==1 else f'lasso/exp/d/{i-2}') if mode==2 else f'logup/exp/{i}',
            native_padding='none;parent-tail-is-member-row' if mode==2 else 'legacy-parent-padding',
            dim_pc=exp_pc(3) if mode==2 and i==0 else pc('','','','',0,0),
            bits=13 if i==0 else 1 if i==1 else 8,
            table=('T[y]=canonicalExpValue(y);8192;K=6891;tail=1' if mode==2 else 'canonical-indexed-pair-8192;K=6891;tail=(K,1)') if i==0 else '{0,1}' if i==1 else '[0,256)',pc=commit,
            relation='LogUp;six-memberships;alpha-after-eight-C' if mode==1 else 'Lasso;query-is-exact-CE;dim-is-exact-Cx;no-new-commitments-or-pair-compression' if i==0 else 'Lasso;same-exp-parent-query-oracle;validity-memory-addresses',
            dependencies=['exp/eight-vector-openings']))
    fields=dict(m=76032,p=131072,pc=exp_pc(0),table=table_id)
    obs.append(obligation('exp/eight-vector-openings','exp-openings',**fields,
                relation='t,E,s,x,d0,d1,d2,d3;eight-exact-oracles;twelve-vector-openings;indexed-pair-compression;canonical-table-ID' if mode==1 else 't,E,s,x,d0,d1,d2,d3;eight-exact-oracles;indexed-E-reuses-Cx-address;no-random-pair-compression;canonical-table-ID'))
    obs.append(obligation('range/uint32-t-E','exp-range',**fields,
                relation='every-t-and-E-row-in-registered-unsigned32-range',dependencies=['range/reconstruction/2']))
    if mode==1:
        for b in range(12):
            r=next(r for r in l['regions'] if r[1]==f'gelu_{b}_t')
            for h in range(512):
                k=512*b+h
                identity=f'direct/local/{k}'
                sources.append(source([3,k,0,0],'gelu-direct',256,[[r[2]+256*h,256,0]],direct_pc(k),
                    [identity,'gkr/gelu-threshold-and-slacks'],bits=1,
                    value_rule='full12-gelu-t-direct-v1;table=[0,1];segment_bits=[1];additive=false;instance=1'))
                obs.append(obligation(identity,'direct-B1',index=k,m=256,p=256,bits=1,
                    table='[0,1];segment_bits=[1];additive=false;instance=1',pc=direct_pc(k),
                    relation='complete-B1-local-proof-and-same-Q-opening;D1-to-C0-required',
                    dependencies=['gkr/gelu-threshold-and-slacks']))
    obs.append(obligation('gkr/gelu-threshold-and-slacks','gelu-gates',pc=main_pc(),
               relation='all-twelve-GeLU-layer-relations;Boolean-membership-does-not-replace-threshold',
               dependencies=[f'gkr/layer/{23*b+j}' for b in range(12) for j in [18,19,20]]))
    sources.append(source([4,0,0,0],'zero-tail',l['N0'],[[l['n0'],l['N0']-l['n0'],0]],main_pc(),
                   ['input/zero-padding'],support=l['N0']-l['n0'],value_rule='mask-C0-indices-[n0,N0);verifier-claim=0'))
    sources.append(source([5,0,0,0],'public-constants',96,[[c,1,0] for c in l['constants']],main_pc(),
                   ['public/constants-binding'],bits=1,value_rule='actual=1;verifier-computes-chi-sum;pad=0'))
    sources.append(source([6,0,0,0],'public-output',32768,[[l['output_offset'],32768,0]],main_pc(),
                   ['public/output-binding'],shape=[32,768,1024],
                   value_rule='public-quantized-Y;row-pad-768-to-1024-with-zero;verifier-MLE;not-last-constraint-output'))
    for j,r in enumerate(l['parameters']):
        obs.append(obligation(f'model/parameter/{j}','model-binding',index=j,m=r[2],p=padded(r[2]),shift=r[1],
            pc=main_pc(),relation='same-approved-anchor-masked-parameter-rows;full-row-equality-and-mixed-row-IPA',table=r[0]))
    for identity,relation in [
        ('public/input-binding','public-X-32x768-row-pad-to-32x1024;anchor-model-binding-same-C0'),
        ('public/output-binding','Y-32x768-row-pad-to-32x1024;D1-to-final-residual-advice'),
        ('public/constants-binding','all-96-used-append-constants-one;D1-to-C0'),
        ('input/zero-padding','C0[n0:N0]=0;no-independent-query-commitment')]:
        obs.append(obligation(identity,'public-binding',pc=main_pc(),relation=relation))
    return dict(schema='route-b-P0-source-plan-v1',policy_id='full12-reviewed-layout-v1',mode=mode,
                model_id=MODEL,implementation_profile=PROFILE,input_pc=main_pc(),sources=sources,obligations=obs)

def check_policy():
    same(hashlib.sha256(pathlib.Path('task9_logs_20261014/full12.anchor.bin').read_bytes()).hexdigest(),ANCHOR,'approved anchor SHA')
    def blob(b):
        return struct.pack('<Q',len(b))+b
    def binding_hash(domain,b):
        return hashlib.sha256(blob(domain.encode())+blob(b)).digest()
    # Recompute canonical table from its mathematical integer definition.
    table=[max(1,math.floor(2**20*math.exp(-j/512)+0.5)) for j in range(6891)]
    values=b''.join(struct.pack('<I',x) for x in table)+struct.pack('<I',1)*(655360-6891)
    spec=b'zkGPT.exp.table.v4;size=655360;t=uint32;floor(2^20*exp(-j/512)+1/2);max(value,1);tail=1;encoding=uint32LE;tail_start=6891'
    table_id=hashlib.sha256(spec+values).digest()
    profile=blob(b'fixed32x768;int32-little-endian;Fr-BN254;input-search(.01);weight-2^-10;LN-default-1,-10,1,-10;source-semantics-v1')
    for name in ['src/neuralNetwork.cpp','src/models.cpp']:
        b=pathlib.Path(name).read_bytes()
        if name.endswith('neuralNetwork.cpp'):
            b=b.replace(b'\r\n',b'\n')
        profile+=blob(name.encode())+binding_hash('zkGPT.quantization.source.v3',b)
    profile+=table_id+struct.pack('<QQ',655360,6891)
    same(binding_hash('zkGPT.quantization.profile.v4',profile).hex(),PROFILE,'independently recomputed implementation profile')
    return table_id.hex()

def verify(output):
    table_id=check_policy()
    l=json.loads((output/'layout.json').read_text())
    expected,fc_ports=scalar_layout()
    for key,value in expected.items():
        same(l[key],value,'full scalar layout '+key)
    same([l['n0'],l['N0'],l['ell'],len(l['regions']),len(l['groups']),len(l['exp']),len(l['constants']),l['output_offset']],
         [195414944,268435456,28,624,10,76032,96,195382176],'full12 invariants after reconstruction')
    same(l['constants'][:8],[163735552,163850378,164374686,164519976,164685884,164800710,165325018,166209764],'first-block constants')
    # Check all constants are actually used by at least one full ordinary port.
    for c in l['constants']:
        need(any(s<=c<s+n for layer in l['layers'] for spans in layer['projection'] for s,n,_ in spans),'unused constant')
        need(all(not (r[1]<=c<r[1]+r[2]) for r in l['parameters']),'constant/parameter alias')
    accesses,mapped=verify_raw(output/'gate-origins.bin',l,fc_ports)
    summary=dict(raw_gate_accesses=accesses,complete_mapped_positions=mapped,regions=624,groups=10,
                 chunks=81,exp_rows=76032,endpoints=576,constants=96,output_offset=l['output_offset'],
                 zero_tail_length=l['N0']-l['n0'])
    for mode,name,count in [(1,'bridge',6735),(2,'lasso',591)]:
        actual=json.loads((output/(name+'.json')).read_text())
        want=expected_plan(mode,l,table_id)
        same(set(actual),set(want),name+' schema fields')
        for key in want:
            if key not in ['sources','obligations']:
                same(actual[key],want[key],name+'/'+key)
        for key in ['sources','obligations']:
            same(len(actual[key]),len(want[key]),name+'/'+key+' count')
            for i,(a,b) in enumerate(zip(actual[key],want[key])):
                same(a,b,f'{name}/{key}/{i}: every field')
        same(len(actual['sources']),count,name+' source count')
        need(len({tuple(s['id']) for s in actual['sources']})==count,'duplicate source ID')
        ids={o['id'] for o in actual['obligations']}
        need(len(ids)==len(actual['obligations']),'duplicate obligation ID')
        need(all(dep in ids for s in actual['sources'] for dep in s['dependencies']),'unresolved source dependency')
        need(all(dep in ids for o in actual['obligations'] for dep in o['dependencies']),'unresolved local dependency')
        same(sum(s['id'][0]==0 for s in actual['sources']),576,'all GKR input endpoints')
        same(sum(o['kind']=='range-chunk' for o in actual['obligations']),81,'all Range chunks')
        same(sum(o['kind']=='exp-membership' for o in actual['obligations']),6,'all Exp memberships')
        # A semantic mutation with a correct total must fail the independent
        # comparator; exercise its actual rejection path on every field class.
        import copy
        edits=[('offset',0,lambda s:s['projection'][0].__setitem__(0,s['projection'][0][0]+1)),
               ('bits',0,lambda s:s.__setitem__('bits',1)),
               ('signed',0,lambda s:s.__setitem__('is_signed',True)),
               ('PC',0,lambda s:s['pc'].__setitem__('oracle','other-C0')),
               ('dependency',0,lambda s:s.__setitem__('dependencies',[])),
               ('constant',-2,lambda s:s['projection'].pop()),
               ('output',-1,lambda s:s['projection'][0].__setitem__(0,0))]
        for label,index,edit in edits:
            # Exercise the same per-source comparator as full verification.
            # Copy only the record under attack, not all 700MB of mappings.
            bad=copy.deepcopy(actual['sources'][index]);edit(bad)
            need(bad!=actual['sources'][index],'negative edit did not change '+label)
            rejected=False
            try:
                same(bad,want['sources'][index],'independent negative '+label)
            except ValueError:
                rejected=True
            need(rejected,'independent comparator accepted '+label)
            print(f'REJECT expected=reject actual=reject independent/{name}/{label}',flush=True)
        summary[name]=dict(sources=count,obligations=len(actual['obligations']))
    print('PASS independent full12 '+json.dumps(summary,sort_keys=True),flush=True)
    return summary

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--emitter',required=True)
    ap.add_argument('--output',required=True,type=pathlib.Path)
    args=ap.parse_args()
    for bad,want in [({'is_signed':1},{'is_signed':True}),({'m':1.0},{'m':1}),
                     ({'id':[0,False]},{'id':[0,0]})]:
        rejected=False
        try:
            same(bad,want,'strict-nested-type-regression')
        except ValueError:
            rejected=True
        need(rejected,'comparator accepted wrong nested JSON type')
    print('PASS expected=reject actual=reject nested bool/int/float substitutions',flush=True)
    # Each run keeps its own evidence; previous CTest output is never replaced.
    base=args.output
    output=base.with_name(base.name+'-'+str(time.time_ns()))
    output.parent.mkdir(parents=True,exist_ok=True)
    started=time.monotonic()
    command=[args.emitter,str(output)]
    before=resource.getrusage(resource.RUSAGE_CHILDREN)
    with output.with_suffix('.stdout').open('x') as stdout,output.with_suffix('.stderr').open('x') as stderr:
        result=subprocess.run(command,stdout=stdout,stderr=stderr)
    after=resource.getrusage(resource.RUSAGE_CHILDREN)
    record=dict(command=command,exit=result.returncode,wall_seconds=time.monotonic()-started,
                cpu_user_seconds=after.ru_utime-before.ru_utime,cpu_system_seconds=after.ru_stime-before.ru_stime,
                peak_rss_kib=after.ru_maxrss,expected='complete full12 emit and independent semantic verification')
    with output.with_suffix('.resource.json').open('x') as f:
        json.dump(record,f,indent=2)
    print(json.dumps(record),flush=True)
    need(result.returncode==0,'public builder/emitter failed; inspect '+str(output)+'.stderr')
    record['independent']=verify(output)
    record['independent_peak_rss_kib']=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    with output.with_suffix('.verified.json').open('x') as f:
        json.dump(record,f,indent=2)
    return 0

if __name__=='__main__':
    try:
        sys.exit(main())
    except Exception as e:
        print('FAIL independent full12: '+str(e),file=sys.stderr,flush=True)
        sys.exit(1)
