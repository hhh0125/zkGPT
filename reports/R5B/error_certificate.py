from decimal import Decimal as D, Context, ROUND_FLOOR, ROUND_CEILING
from fractions import Fraction as F
from pathlib import Path
import json,time
lo=Context(prec=80,rounding=ROUND_FLOOR);hi=Context(prec=80,rounding=ROUND_CEILING)
A=D(3)/16;B=D(217)/256;C=D(1000605)/1048576;T=D(615)/256;ONE=D(1);TWO=D(2);K=180
assert F(7977,10000)**2*F(22,7)<2 and F(7980,10000)**2*F(333,106)>2
assert F(3,16)*F(615,256)**2-F(217,256)*F(615,256)+F(1000605,1048576)==0
Lf=1+F(1,2)*(3*F(3,16)*F(615,256)**2+2*F(217,256)*F(615,256)+F(1000605,1048576));assert Lf<F(11,2)
Lg=1+F(8,5)*(1/(F(158,100)*F(27,10))+F(135,1000)*27/(F(158,100)**3*F(27,10)**3));assert Lg<F(3,2)
def exp_interval(z0,z1):
 tl=th=sl=sh=ONE
 for k in range(1,K+1):
  tl=lo.divide(lo.multiply(tl,z0),D(k));sl=lo.add(sl,tl)
  th=hi.divide(hi.multiply(th,z1),D(k));sh=hi.add(sh,th)
 nxt=hi.divide(hi.multiply(th,z1),D(K+1));ratio=hi.divide(z1,D(K+2));den=lo.subtract(ONE,ratio);assert den>0
 return sl,hi.add(sh,hi.divide(nxt,den))
start=time.monotonic();maximum=D(0);lower_max=D(0);where='';spacing=4096
for i in range(8*spacing+1):
 u=lo.divide(D(i),D(spacing));u2=lo.multiply(u,u);u3=lo.multiply(u2,u)
 f=lo.add(u,lo.multiply(D('0.044715'),u3))
 z0=lo.multiply(D('1.5954'),f);z1=hi.multiply(D('1.5960'),f)
 el,eh=exp_interval(z0,z1);dl=lo.divide(u,hi.add(ONE,eh));dh=hi.divide(u,lo.add(ONE,el))
 if u<=T:
  cl=lo.divide(lo.add(lo.subtract(lo.multiply(A,u3),hi.multiply(B,u2)),lo.multiply(C,u)),TWO)
  ch=hi.divide(hi.add(hi.subtract(hi.multiply(A,u3),lo.multiply(B,u2)),hi.multiply(C,u)),TWO)
 else:cl=ch=D(0)
 low=lo.subtract(dl,ch);high=hi.subtract(dh,cl);upper=max(low.copy_abs(),high.copy_abs());lower=D(0) if low<=0<=high else min(low.copy_abs(),high.copy_abs())
 if upper>maximum:maximum=upper;where=str(u)
 lower_max=max(lower_max,lower)
 if i%8192==0:print('interval-grid',i,'of',8*spacing,flush=True)
bound=hi.add(maximum,hi.divide(D(7),D(2*spacing)))
tail_exp,_=exp_interval(D('12.64'),D('12.64'));tail=hi.divide(D(8),tail_exp)
record={
 'function':'R5B continuous engineering z-GELU F versus GPT-2 nominal tanh-GELU g',
 'domain':'[-8,8]; outside this interval use the explicit analytic tail bound',
 'method':'80-digit directed Decimal arithmetic, positive Taylor exp enclosure through degree 180 with geometric tail bound; alpha=sqrt(2/pi) bracket [0.7977,0.7980] from 333/106<pi<22/7; real-continuous piecewise derivative bound |(F-g)\'|<7',
 'grid_spacing':'1/4096','grid_points_nonnegative':8*spacing+1,'symmetry':'F-g is even',
 'max_grid_error_lower':str(lower_max),'max_grid_error_upper':str(maximum),'max_upper_near_abs_x':where,
 'lipschitz_extension':'7/8192','continuous_error_upper_on_domain':str(bound),
 'tail_error_upper_abs_x_ge_8':str(tail),
 'output_quantization_error_upper':'5/1024 at fixed real input x=(5/512)q',
 'quantized_error_upper_at_lattice_inputs':str(hi.add(max(bound,tail),D(5)/1024)),
 'threshold_correction_exactly_zero':True,'coefficient_fitting':'engineering continuity adjustment; not a new minimax fit or author original coefficient claim',
 'excludes':['input quantization against arbitrary floating input','checkpoint/weight quantization','LayerNorm/root approximation','cross-block error propagation','floating executable bit equivalence'],
 'seconds':time.monotonic()-start}
Path('reports/R5B/GELU_ERROR_CERTIFICATE.json').write_text(json.dumps(record,indent=2));print(json.dumps(record,indent=2))
