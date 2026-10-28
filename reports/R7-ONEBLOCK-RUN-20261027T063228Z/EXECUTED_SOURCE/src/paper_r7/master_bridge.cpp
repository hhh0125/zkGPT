#include "master_bridge.hpp"
namespace zkgpt::r7::master3 {
namespace {
Hidden handle(IO&io,const std::string&role,const Fr&v,Random*rng){Hidden h;Bytes b;if(io.prover()){h=hide(io.session(),role,v,*rng);b=encode(h.handle);}b=io.message(role+"/commitment",b);h.handle=decodeHandle(b);validate(h.handle,io.session(),role);return h;}
Hidden known(IO&io,const std::string&role,const Fr&v){Hidden h;h.handle.session=io.session();h.handle.role=role;h.value=v;h.handle.point=parameters(1).scalar*v;return h;}
void linear(IO&io,const std::string&role,const std::vector<Hidden>&h,const std::vector<Fr>&coeff,Random*rng){Relations r;r.role=role;for(auto&x:h)r.handles.push_back(x.handle);r.linear={coeff};sigma(io,r,io.prover()?&h:nullptr,rng);}
class Terminal final:public TerminalOpeningBackend {
 const Master&m_;const Secret*secret_;const Hidden&child_;const std::vector<Fr>&kernel_;Fr bias_;std::string role_;
 public:Terminal(const Master&m,const Secret*s,const Hidden&child,const std::vector<Fr>&k,const Fr&b,std::string role):m_(m),secret_(s),child_(child),kernel_(k),bias_(b),role_(std::move(role)){}
 TerminalBackendKind kind()const override{return TerminalBackendKind::MasterBridgeA3V1;}
 void authenticate(IO&io,const Hidden&initial,const std::vector<Hidden>&ends,const std::vector<Fr>&point,Random*rng)override{
  need(ends.size()==2,"M3 bridge terminal factor count");auto b=known(io,role_+"/bias",bias_);linear(io,role_+"/initial-link",{initial,child_,b},{Fr(1),Fr(-1),Fr(1)},rng);
  auto k=known(io,role_+"/public-kernel",r1::mle(kernel_,point));linear(io,role_+"/kernel-link",{ends[1],k},{Fr(1),Fr(-1)},rng);
  openMaster(io,m_,{point,ends[0].handle},secret_,io.prover()?&ends[0]:nullptr,rng,role_+"/terminal");
 }
};
}
void bindChild(IO&io,RegisteredMaster&registered,const PC&child,const Gather&map,const Secret*master,const PCSecret*secret,Random*rng,const std::string&attack){
 const auto&m=registered.get(io);need(m.spec.N==4096 && m.spec.scope=="FC-source-projection","M3 Bridge INCOMPLETE beyond registered bounded projection");validate(map,m.spec.actual);validate(child);need(child.spec.actual==map.child_size && child.spec.padded==map.child_size,"M3 child map dimension");
 auto role="M3/bridge/"+map.id;Writer b;b.text(suite);b.text(identity(m));b.text(m.spec.layout);b.raw(encodeGather(map));auto pc=encode(child);b.number(pc.size());b.raw(pc);bindPublic(io,role+"/source-and-child",b.b);
 std::vector<Fr>point;for(unsigned i=0;i<r1::log2exact(map.child_size);++i)point.push_back(io.coin(role+"/child-point/"+std::to_string(i)));
 auto evaluation=handle(io,role+"/child-evaluation",io.prover()?r1::mle(secret->values,point):Fr(0),rng);stage2::ScopedIO childIO(io,role+"/child");hyrax_a3::opening(childIO,child,{point,evaluation.handle},secret,io.prover()?&evaluation:nullptr,rng);
 Fr bias;auto kernel=gatherKernel(map,point,m.spec.N,bias);if(io.prover() && attack=="duplicate-coefficient")for(auto&t:map.terms)if(t.source==3){kernel[3]-=r1::weights(point)[t.child];break;}
 SumcheckWitness witness;if(io.prover()){need(master && secret && rng,"M3 Bridge missing witness");witness.claim=evaluation.value-bias;witness.factors.assign(2,std::vector<Fr>(m.spec.N,Fr(0)));for(std::size_t i=0;i<m.spec.actual;++i)witness.factors[0][i]=master->value(i);witness.factors[1]=kernel;}
 Terminal backend(m,master,evaluation,kernel,bias,role);committedSumcheckRelation(io,{m.spec.N,2,role+"/reduction"},io.prover()?&witness:nullptr,backend,rng);
}
}
