#include "stage2_sumcheck.hpp"
namespace zkgpt::r7::stage2 {
namespace {
class A3Terminals final:public TerminalOpeningBackend {
 RegisteredCW&cw_;const std::vector<PC>&factors_;const PC&output_;const std::vector<const PCSecret*>&secrets_;const PCSecret*output_secret_;
 public:A3Terminals(RegisteredCW&cw,const std::vector<PC>&factors,const PC&out,const std::vector<const PCSecret*>&secrets,const PCSecret*os):cw_(cw),factors_(factors),output_(out),secrets_(secrets),output_secret_(os){}
 TerminalBackendKind kind()const override{return TerminalBackendKind::HyraxA3Stage2V1;}
 void authenticate(IO&io,const Hidden&initial,const std::vector<Hidden>&endpoints,const std::vector<Fr>&point,Random*rng)override{
  need(endpoints.size()==factors_.size() && point.size()==r1::log2exact(cw_.approved().pc.spec.padded),"S2 terminal relation dimensions");
  // These are the SAME full Hidden objects/Handles used by the recurrence and
  // product Sigma. No scalar regeneration or role-only substitution occurs.
  std::vector<Hidden>out{initial};cw_.openInference(output_,"sumcheck-output",{{{},initial.handle}},output_secret_,io.prover()?&out:nullptr,rng);
  for(std::size_t k=0;k<endpoints.size();++k){std::vector<Hidden>h{endpoints[k]};std::vector<Query>q{{point,endpoints[k].handle}};
   if(k==1)cw_.openFixed("sumcheck-weight",q,io.prover()?secrets_[k]:nullptr,io.prover()?&h:nullptr,rng);
   else cw_.openInference(factors_[k],"sumcheck-factor"+std::to_string(k),q,io.prover()?secrets_[k]:nullptr,io.prover()?&h:nullptr,rng);
  }
 }
};
}
void committedA3Sumcheck(IO&io,RegisteredCW&cw,const std::vector<PC>&factors,const PC&output,const std::vector<const PCSecret*>&secrets,const PCSecret*output_secret,const SumcheckWitness*witness,Random*rng,const std::string&attack){
 need(factors.size()==2 || factors.size()==3,"S2 degree2/3 required");need(encode(factors[1])==encode(cw.approved().pc),"S2 Sumcheck fixed weight identity mismatch");
 const auto n=cw.approved().pc.spec.actual;Writer publicInput;publicInput.text(suite);publicInput.text(hyrax_a3::suite);publicInput.text(registryDigest(cw.approved()));publicInput.number(n);publicInput.number(factors.size());
 for(std::size_t k=0;k<factors.size();++k){validate(factors[k]);need(factors[k].spec.actual==n && factors[k].spec.padded==n,"S2 factor lengths/zero-tail INCOMPLETE");if(k!=1)need(factors[k].spec.role=="S2/activation/"+std::to_string(k),"S2 factor object mismatch");auto b=encode(factors[k]);publicInput.number(b.size());publicInput.raw(b);}
 validate(output);need(output.spec.role=="S2/output" && output.spec.actual==1 && output.spec.padded==1,"S2 output commitment identity");publicInput.raw(encode(output));bindPublic(io,"S2/Sumcheck/objects",publicInput.b);
 if(io.prover())need(secrets.size()==factors.size(),"S2 Sumcheck private operands missing");
 A3Terminals backend(cw,factors,output,secrets,output_secret);
 committedSumcheckRelation(io,{n,factors.size(),"S2/committed-sumcheck"},witness,backend,rng,attack);
}
}
