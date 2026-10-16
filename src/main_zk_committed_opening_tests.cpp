#include "zk_committed_linear_opening.hpp"
#include "zk_linear_opening.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>

namespace cy = zkproto::committed;
using zkproto::Fr; using zkproto::G1;
namespace {
int passed=0, failed=0;
void check(bool ok,const std::string& why) { if(!ok)throw std::runtime_error(why); }
template<class F> void test(const std::string& name,F fn) {
    try { fn();passed++;std::cout<<"PASS "<<name<<'\n'; }
    catch(const std::exception& e) { failed++;std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n'; }
}
template<class F> void throws(F fn) {
    bool rejected=false;try {fn();}catch(const std::exception&){rejected=true;}
    check(rejected,"expected local/decode rejection");
}
std::vector<Fr> randomFields(std::size_t n) {
    std::vector<Fr> v(n,Fr(0));for(auto& x:v)x=zkproto::sampleUniformScalar();return v;
}
Fr manualDot(const std::vector<Fr>& a,const std::vector<Fr>& b) {
    Fr v=0;for(std::size_t i=0;i<a.size();i++)v+=a[i]*b.at(i);return v;
}
struct Fixture { cy::Parameters pp;cy::Statement s;zkproto::SecretWitness w;Fr eta=Fr(0);cy::Proof p; };
// Secret fixture vectors and blinds stay in process memory; emission is public-only.
Fixture fixture(std::size_t n,bool b0=false,bool w0=false,bool y0=false) {
    Fixture f;f.pp=cy::makeParameters(n);auto w=randomFields(n);
    if(w0)std::fill(w.begin(),w.end(),Fr(0));
    auto C=zkproto::randomCommit(f.pp.vector_parameters,w);f.w=C.witness;
    f.s.commitment=C.point;f.s.context="task11/session-a/layout-v1";
    f.s.coefficients=randomFields(n);f.s.coefficients[0]=Fr(1);
    if(b0)std::fill(f.s.coefficients.begin(),f.s.coefficients.end(),Fr(0));
    if(y0){std::fill(f.s.coefficients.begin(),f.s.coefficients.end(),Fr(0));f.s.coefficients[0]=w[1];f.s.coefficients[1]=-w[0];}
    auto D=cy::randomScalarCommit(f.pp,manualDot(w,f.s.coefficients));
    f.s.evaluation_commitment=D.point;f.eta=D.blind;
    f.p=cy::prove(f.pp,f.s,f.w,f.eta);return f;
}
void reject(const Fixture& f,const cy::Statement& s,const cy::Proof& p) {
    std::string error;check(!cy::verify(f.pp,s,p,&error),"invalid proof accepted");check(!error.empty(),"no rejection diagnostic");
}
template<class F> void entropyFailure(F fn) {
    auto pid=fork();check(pid>=0,"fork failed");
    if(pid==0) {
        sock_filter filter[]={
            BPF_STMT(BPF_LD|BPF_W|BPF_ABS,offsetof(seccomp_data,nr)),
            BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,SYS_getrandom,0,1),
            BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_ERRNO|EIO),
            BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_ALLOW)};
        sock_fprog program{static_cast<unsigned short>(sizeof(filter)/sizeof(filter[0])),filter};
        if(prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)!=0 || prctl(PR_SET_SECCOMP,SECCOMP_MODE_FILTER,&program)!=0)_exit(77);
        try { fn();_exit(1); } catch(const std::exception&) { _exit(0); }
    }
    int status=0;check(waitpid(pid,&status,0)==pid,"waitpid failed");
    check(WIFEXITED(status)&&WEXITSTATUS(status)==0,"entropy did not fail closed or seccomp unavailable");
}
// TEST ONLY: creates a well-shaped FS attempt despite wrong openings.
// Same mask d and response z in both equations. No production bypass API.
cy::Proof maliciousAttempt(const Fixture& f,const zkproto::SecretWitness& wc,const Fr& eta) {
    auto d=randomFields(f.w.values.size());Fr a=zkproto::sampleUniformScalar(),e=zkproto::sampleUniformScalar();
    cy::Proof p;p.responses.resize(d.size(),Fr(0));
    p.nonce_commitment=zkproto::commit(f.pp.vector_parameters,d,a);
    p.nonce_evaluation_commitment=cy::scalarCommit(f.pp,manualDot(f.s.coefficients,d),e);
    Fr c=cy::challenge(f.pp,f.s,p);
    for(std::size_t i=0;i<d.size();i++)p.responses[i]=d[i]+c*wc.values[i];
    p.blind_response=a+c*wc.blind;p.evaluation_blind_response=e+c*eta;return p;
}
void firstPassesSecondFails(const Fixture& f,const cy::Proof& p) {
    Fr c=cy::challenge(f.pp,f.s,p);
    check(zkproto::commit(f.pp.vector_parameters,p.responses,p.blind_response)==p.nonce_commitment+f.s.commitment*c,"test attempt fails first equation");
    check(cy::scalarCommit(f.pp,manualDot(f.s.coefficients,p.responses),p.evaluation_blind_response)!=p.nonce_evaluation_commitment+f.s.evaluation_commitment*c,"test attempt accidentally satisfies second equation");
    reject(f,f.s,p);
}
void redTests() {
    test("scalar_commitment_random_blind",[]{auto pp=cy::makeParameters(2);auto D=cy::randomScalarCommit(pp,Fr(-5));check(!D.point.isZero(),"missing scalar commitment");check(D.point==pp.value_generator*Fr(-5)+pp.hiding_generator*D.blind,"scalar equation");});
    for(std::size_t n:{2u,4u})test("committed_positive_n"+std::to_string(n),[=]{auto f=fixture(n);check(cy::verify(f.pp,f.s,f.p),"missing shared-preimage proof");});
}
void units() {
    redTests();
    test("generators_verifier_derived_independent_labels",[]{
        auto p=cy::makeParameters(4);cy::validateParameters(p);
        auto q=p;q.value_generator=p.hiding_generator;throws([&]{cy::validateParameters(q);});
        q=p;q.hiding_generator=p.vector_parameters.generators[0];throws([&]{cy::validateParameters(q);});
        q=p;q.scalar_domain="wrong";throws([&]{cy::validateParameters(q);});
    });
    test("scalar_zero_value_and_negative_integer",[]{
        auto pp=cy::makeParameters(2);Fr eta=zkproto::sampleUniformScalar();
        check(cy::scalarCommit(pp,Fr(0),eta)==pp.hiding_generator*eta,"zero scalar equation");
        auto D=cy::scalarCommit(pp,Fr(-5),eta);check(D==pp.value_generator*Fr(-5)+pp.hiding_generator*eta,"negative scalar equation");
        Fr last=-1;check(last+Fr(1)==Fr(0),"negative Fr reduction");
    });
    test("same_w_different_rho",[]{auto f=fixture(4);auto C=zkproto::randomCommit(f.pp.vector_parameters,f.w.values);check(C.point!=f.s.commitment && C.witness.blind!=f.w.blind,"rho reused");auto s=f.s;s.commitment=C.point;auto p=cy::prove(f.pp,s,C.witness,f.eta);check(cy::verify(f.pp,s,p),"new rho proof rejected");});
    test("same_w_different_eta",[]{auto f=fixture(4);auto D=cy::randomScalarCommit(f.pp,manualDot(f.s.coefficients,f.w.values));check(D.point!=f.s.evaluation_commitment && D.blind!=f.eta,"eta reused");auto s=f.s;s.evaluation_commitment=D.point;check(cy::verify(f.pp,s,cy::prove(f.pp,s,f.w,D.blind)),"new eta proof rejected");});
    test("zero_witness",[]{auto f=fixture(4,false,true);check(cy::verify(f.pp,f.s,f.p),"zero witness rejected");});
    test("negative_witness_and_zero_coefficient",[]{auto f=fixture(4);f.w.values[0]=Fr(-5);f.s.coefficients[1]=Fr(0);f.s.commitment=zkproto::commit(f.pp.vector_parameters,f.w.values,f.w.blind);f.s.evaluation_commitment=cy::scalarCommit(f.pp,manualDot(f.s.coefficients,f.w.values),f.eta);check(cy::verify(f.pp,f.s,cy::prove(f.pp,f.s,f.w,f.eta)),"negative/zero rejected");});
    for(std::size_t n:{2u,4u})test("privacy_b0_n"+std::to_string(n),[=]{
        auto f=fixture(n,true);check(cy::verify(f.pp,f.s,f.p),"b0 rejected");
        auto c=cy::challenge(f.pp,f.s,f.p);auto guessed=f.p.responses;for(auto& v:guessed)v/=c;
        check(guessed!=f.w.values,"unmasked witness recovery");
        check(f.p.nonce_evaluation_commitment==f.pp.hiding_generator*(f.p.evaluation_blind_response-c*f.eta),"b0 masked group nonce relation");
    });
    test("hidden_y0_nonzero_b",[]{auto f=fixture(2,false,false,true);check(manualDot(f.s.coefficients,f.w.values).isZero(),"orthogonal fixture");check(cy::verify(f.pp,f.s,f.p),"hidden zero rejected");});
    test("hidden_y_nonzero",[]{auto f=fixture(2);check(!manualDot(f.s.coefficients,f.w.values).isZero(),"negligible random dot collision");check(cy::verify(f.pp,f.s,f.p),"nonzero hidden value rejected");});
    test("repeated_statement_fresh_nonces",[]{auto f=fixture(4);std::set<cy::Bytes> proofs;proofs.insert(cy::encodeProof(f.p));for(int i=0;i<8;i++){auto p=cy::prove(f.pp,f.s,f.w,f.eta);check(cy::verify(f.pp,f.s,p),"repeat rejected");check(p.nonce_commitment!=f.p.nonce_commitment && p.nonce_evaluation_commitment!=f.p.nonce_evaluation_commitment,"nonce reused");check(proofs.insert(cy::encodeProof(p)).second,"proof repeated");}});
    test("roundtrip_n2_n4",[]{for(std::size_t n:{2u,4u}){auto f=fixture(n);auto s=cy::encodeStatement(f.s),p=cy::encodeProof(f.p);auto sd=cy::decodeStatement(s);auto pd=cy::decodeProof(p);check(cy::encodeStatement(sd)==s&&cy::encodeProof(pd)==p,"roundtrip not canonical");check(cy::verify(f.pp,sd,pd),"roundtrip rejected");}});
    test("identity_commitments_legal",[]{auto f=fixture(2,false,true);f.w.blind=Fr(0);f.eta=Fr(0);f.s.commitment.clear();f.s.evaluation_commitment.clear();auto p=cy::prove(f.pp,f.s,f.w,f.eta);check(cy::verify(f.pp,cy::decodeStatement(cy::encodeStatement(f.s)),p),"identity statement rejected");});
    test("identity_first_messages_codec",[]{cy::Proof p;p.responses.assign(2,Fr(0));auto q=cy::decodeProof(cy::encodeProof(p));check(q.nonce_commitment.isZero()&&q.nonce_evaluation_commitment.isZero(),"identity nonce rejected");});
    auto f=fixture(4);
    test("entropy_failure_scalar_blind",[&]{entropyFailure([&]{cy::randomScalarCommit(f.pp,Fr(1));});});
    test("entropy_failure_prover_masks",[&]{entropyFailure([&]{cy::prove(f.pp,f.s,f.w,f.eta);});});
    test("crypto_changed_C",[&]{auto s=f.s;s.commitment+=f.pp.vector_parameters.generators[0];reject(f,s,f.p);});
    test("crypto_changed_D",[&]{auto s=f.s;s.evaluation_commitment+=f.pp.value_generator;reject(f,s,f.p);});
    test("crypto_changed_b",[&]{auto s=f.s;s.coefficients[0]+=Fr(1);reject(f,s,f.p);});
    test("crypto_changed_A_C",[&]{auto p=f.p;p.nonce_commitment+=f.pp.vector_parameters.generators[0];reject(f,f.s,p);});
    test("crypto_changed_A_D",[&]{auto p=f.p;p.nonce_evaluation_commitment+=f.pp.value_generator;reject(f,f.s,p);});
    test("crypto_changed_z",[&]{auto p=f.p;p.responses[0]+=Fr(1);reject(f,f.s,p);});
    test("crypto_changed_z_rho",[&]{auto p=f.p;p.blind_response+=Fr(1);reject(f,f.s,p);});
    test("crypto_changed_z_eta",[&]{auto p=f.p;p.evaluation_blind_response+=Fr(1);reject(f,f.s,p);});
    test("crypto_mixed_commitments_existing_proof",[&]{auto other=fixture(4);auto s=f.s;s.evaluation_commitment=other.s.evaluation_commitment;reject(f,s,f.p);});
    test("crypto_wrong_shared_w_second_equation",[&]{auto bad=f;auto w2=f.w.values;w2[0]+=Fr(1);bad.s.evaluation_commitment=cy::scalarCommit(f.pp,manualDot(f.s.coefficients,w2),f.eta);firstPassesSecondFails(bad,maliciousAttempt(bad,f.w,f.eta));});
    test("crypto_wrong_eta_second_equation",[&]{firstPassesSecondFails(f,maliciousAttempt(f,f.w,f.eta+Fr(1)));});
    test("crypto_changed_context",[&]{auto s=f.s;s.context="task11/session-b/layout-v1";reject(f,s,f.p);});
    test("crypto_same_context_replay_valid",[&]{check(cy::verify(f.pp,f.s,f.p),"same-statement replay should be valid");});
    test("verifier_wrong_generators",[&]{auto pp=f.pp;pp.value_generator=pp.vector_parameters.generators[0];check(!cy::verify(pp,f.s,f.p),"untrusted bases accepted");});
    test("verifier_domains_and_versions",[&]{auto s=f.s;s.scalar_domain="wrong";reject(f,s,f.p);s=f.s;s.vector_domain="wrong";reject(f,s,f.p);auto p=f.p;p.domain="wrong";reject(f,f.s,p);p=f.p;p.version=3;reject(f,f.s,p);s=f.s;s.version=2;reject(f,s,f.p);});
    test("local_wrong_C_opening",[&]{auto w=f.w;w.values[0]+=Fr(1);throws([&]{cy::prove(f.pp,f.s,w,f.eta);});});
    test("local_wrong_eta",[&]{throws([&]{cy::prove(f.pp,f.s,f.w,f.eta+Fr(1));});});
    test("local_mixed_w",[&]{auto s=f.s;auto w=f.w.values;w[0]+=Fr(1);s.evaluation_commitment=cy::scalarCommit(f.pp,manualDot(s.coefficients,w),f.eta);throws([&]{cy::prove(f.pp,s,f.w,f.eta);});});
    test("local_wrong_dimensions",[&]{auto w=f.w;w.values.pop_back();throws([&]{cy::prove(f.pp,f.s,w,f.eta);});auto s=f.s;s.coefficients.resize(2);throws([&]{cy::prove(f.pp,s,f.w,f.eta);});});
    test("unsupported_lengths",[]{for(std::size_t n:{0u,1u,3u,8u,131072u})throws([=]{cy::makeParameters(n);});});
    auto raw=cy::encodeProof(f.p);std::size_t poff=16+f.p.domain.size();
    auto sraw=cy::encodeStatement(f.s);
    test("decode_truncated_every_offset",[&]{for(std::size_t i=0;i<raw.size();i++){cy::Bytes v(raw.begin(),raw.begin()+i);throws([&]{cy::decodeProof(v);});}});
    test("decode_trailing",[&]{auto v=raw;v.push_back(0);throws([&]{cy::decodeProof(v);});});
    test("decode_magic_and_version",[&]{auto v=raw;v[0]^=1;throws([&]{cy::decodeProof(v);});v=raw;v[4]=2;throws([&]{cy::decodeProof(v);});});
    test("decode_domain",[&]{auto v=raw;v[16]^=1;throws([&]{cy::decodeProof(v);});});
    test("decode_invalid_G1_both_nonces",[&]{for(auto off:{poff,poff+32}){auto v=raw;std::fill(v.begin()+off,v.begin()+off+32,255);throws([&]{cy::decodeProof(v);});}});
    test("decode_noncanonical_Fr_all_responses",[&]{for(std::size_t off=poff+64;off<raw.size();off+=32){auto v=raw;std::fill(v.begin()+off,v.begin()+off+32,255);throws([&]{cy::decodeProof(v);});}});
    test("decode_exact_Fr_modulus",[&]{auto v=raw;std::array<std::uint8_t,32> r{};Fr last=-1;last.serialize(r.data(),32);for(auto& x:r){if(++x!=0)break;}std::copy(r.begin(),r.end(),v.end()-32);throws([&]{cy::decodeProof(v);});});
    test("decode_n_and_domain_length_bounds",[&]{auto v=raw;std::fill(v.begin()+8,v.begin()+12,255);throws([&]{cy::decodeProof(v);});v=raw;std::fill(v.begin()+12,v.begin()+16,255);throws([&]{cy::decodeProof(v);});});
    test("decode_message_cap",[]{cy::Bytes v(4097,0);throws([&]{cy::decodeProof(v);});throws([&]{cy::decodeStatement(v);});});
    test("encode_oversized_context",[&]{auto s=f.s;s.context=std::string(129,'x');throws([&]{cy::encodeStatement(s);});});
    test("context_empty_and_max_length",[&]{for(auto context:{std::string(),std::string(128,'x')}){auto s=f.s;s.context=context;auto p=cy::prove(f.pp,s,f.w,f.eta);check(cy::verify(f.pp,cy::decodeStatement(cy::encodeStatement(s)),p),"valid context rejected");}});
    test("statement_truncated_and_trailing",[&]{auto v=sraw;v.pop_back();throws([&]{cy::decodeStatement(v);});v=sraw;v.push_back(0);throws([&]{cy::decodeStatement(v);});});
    test("statement_truncated_every_offset",[&]{for(std::size_t i=0;i<sraw.size();i++){cy::Bytes v(sraw.begin(),sraw.begin()+i);throws([&]{cy::decodeStatement(v);});}});
    test("statement_unique_header_bounds",[&]{
        for(auto off:{0u,4u,8u}){auto v=sraw;v[off]^=1;throws([&]{cy::decodeStatement(v);});}
        for(auto off:{std::size_t(12),16+f.s.vector_domain.size(),20+f.s.vector_domain.size()+f.s.scalar_domain.size()}){auto v=sraw;std::fill(v.begin()+off,v.begin()+off+4,255);throws([&]{cy::decodeStatement(v);});}
    });
    test("statement_noncanonical_Fr",[&]{auto v=sraw;std::fill(v.end()-32,v.end(),255);throws([&]{cy::decodeStatement(v);});});
    test("statement_invalid_points",[&]{std::size_t off=16+f.s.vector_domain.size()+4+f.s.scalar_domain.size()+4+f.s.context.size();for(auto pos:{off,off+32}){auto v=sraw;std::fill(v.begin()+pos,v.begin()+pos+32,255);throws([&]{cy::decodeStatement(v);});}});
    test("public_y_codec_cannot_cross_decode",[&]{throws([&]{zkproto::decodeStatement(sraw);});throws([&]{zkproto::decodeProof(raw);});auto old=zkproto::Statement();old.coefficients.assign(4,Fr(0));throws([&]{cy::decodeStatement(zkproto::encodeStatement(old));});auto op=zkproto::LinearOpeningProof();op.responses.assign(4,Fr(0));throws([&]{cy::decodeProof(zkproto::encodeProof(op));});});
    test("challenge_nonzero_and_binds_both_messages",[&]{auto c=cy::challenge(f.pp,f.s,f.p);check(!c.isZero()&&c.isValid(),"invalid challenge");auto p=f.p;p.nonce_evaluation_commitment+=f.pp.value_generator;check(cy::challenge(f.pp,f.s,p)!=c,"A_D not bound");p=f.p;p.nonce_commitment+=f.pp.value_generator;check(cy::challenge(f.pp,f.s,p)!=c,"A_C not bound");});
    test("shared_preimage_special_soundness",[&]{
        auto d=randomFields(4);Fr a=zkproto::sampleUniformScalar(),e=zkproto::sampleUniformScalar();
        G1 ac=zkproto::commit(f.pp.vector_parameters,d,a),ad=cy::scalarCommit(f.pp,manualDot(f.s.coefficients,d),e);
        Fr c=Fr(7),cp=Fr(11);auto z=d,zp=d;
        for(std::size_t i=0;i<4;i++){z[i]+=c*f.w.values[i];zp[i]+=cp*f.w.values[i];}
        Fr zr=a+c*f.w.blind,zrp=a+cp*f.w.blind,ze=e+c*f.eta,zep=e+cp*f.eta;
        for(bool second:{false,true}){auto& zs=second?zp:z;Fr cc=second?cp:c;check(zkproto::commit(f.pp.vector_parameters,zs,second?zrp:zr)==ac+f.s.commitment*cc,"interactive first equation");check(cy::scalarCommit(f.pp,manualDot(f.s.coefficients,zs),second?zep:ze)==ad+f.s.evaluation_commitment*cc,"interactive second equation");}
        auto w=z;for(std::size_t i=0;i<4;i++)w[i]=(z[i]-zp[i])/(c-cp);
        Fr rho=(zr-zrp)/(c-cp),eta=(ze-zep)/(c-cp);
        check(w==f.w.values&&rho==f.w.blind&&eta==f.eta,"common witness extraction");
        check(zkproto::commit(f.pp.vector_parameters,w,rho)==f.s.commitment,"extracted C relation");check(cy::scalarCommit(f.pp,manualDot(f.s.coefficients,w),eta)==f.s.evaluation_commitment,"extracted D relation");
    });
    test("HVZK_public_only_equations_b0_and_nonzero",[]{
        for(bool b0:{false,true}){auto f=fixture(4,b0);auto x=f.s;auto pp=f.pp; // simulator below takes only public data
            auto z=randomFields(4);Fr zr=zkproto::sampleUniformScalar(),ze=zkproto::sampleUniformScalar(),c=Fr(13);
            auto ac=zkproto::commit(pp.vector_parameters,z,zr)-x.commitment*c;
            auto ad=cy::scalarCommit(pp,manualDot(x.coefficients,z),ze)-x.evaluation_commitment*c;
            check(zkproto::commit(pp.vector_parameters,z,zr)==ac+x.commitment*c,"public simulator first equation");check(cy::scalarCommit(pp,manualDot(x.coefficients,z),ze)==ad+x.evaluation_commitment*c,"public simulator second equation");
        }
    });
}
void writeNew(const std::filesystem::path& p,const cy::Bytes& b) {
    int fd=open(p.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);check(fd>=0,"artifact exists or creation failed");
    std::size_t pos=0;while(pos<b.size()){auto n=write(fd,b.data()+pos,b.size()-pos);if(n<0&&errno==EINTR)continue;if(n<=0){close(fd);throw std::runtime_error("artifact write failed");}pos+=static_cast<std::size_t>(n);}check(close(fd)==0,"artifact close failed");
}
void emit(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);auto f=fixture(4);
    auto pair=[&](const std::string& name,const cy::Statement& s,const cy::Proof& p){writeNew(dir/(name+".statement.bin"),cy::encodeStatement(s));writeNew(dir/(name+".proof.bin"),cy::encodeProof(p));};
    pair("positive-n4",f.s,f.p);auto n2=fixture(2);pair("positive-n2",n2.s,n2.p);
    auto b0=fixture(4,true);pair("positive-b0",b0.s,b0.p);auto w0=fixture(4,false,true);pair("positive-zero-w",w0.s,w0.p);auto y0=fixture(2,false,false,true);pair("positive-zero-y",y0.s,y0.p);
    auto repeat=cy::prove(f.pp,f.s,f.w,f.eta);pair("positive-repeat",f.s,repeat);
    auto s=f.s;s.commitment+=f.pp.vector_parameters.generators[0];pair("wrong-C",s,f.p);
    s=f.s;s.evaluation_commitment+=f.pp.value_generator;pair("wrong-D",s,f.p);
    s=f.s;s.coefficients[0]+=Fr(1);pair("wrong-b",s,f.p);
    s=f.s;s.context="task11/session-b/layout-v1";pair("wrong-context",s,f.p);
    auto p=f.p;p.nonce_commitment+=f.pp.vector_parameters.generators[0];pair("wrong-A-C",f.s,p);
    p=f.p;p.nonce_evaluation_commitment+=f.pp.value_generator;pair("wrong-A-D",f.s,p);
    p=f.p;p.responses[0]+=Fr(1);pair("wrong-z",f.s,p);
    p=f.p;p.blind_response+=Fr(1);pair("wrong-z-rho",f.s,p);
    p=f.p;p.evaluation_blind_response+=Fr(1);pair("wrong-z-eta",f.s,p);
    auto other=fixture(4);s=f.s;s.evaluation_commitment=other.s.evaluation_commitment;pair("spliced-D",s,f.p);
    auto bad=f;auto w2=f.w.values;w2[0]+=Fr(1);bad.s.evaluation_commitment=cy::scalarCommit(f.pp,manualDot(f.s.coefficients,w2),f.eta);
    p=maliciousAttempt(bad,f.w,f.eta);firstPassesSecondFails(bad,p);pair("wrong-shared-w",bad.s,p);
    p=maliciousAttempt(f,f.w,f.eta+Fr(1));firstPassesSecondFails(f,p);pair("wrong-eta-forged",f.s,p);
    auto raw=cy::encodeProof(f.p);
    auto rawcase=[&](const std::string& name,const cy::Bytes& v){writeNew(dir/(name+".statement.bin"),cy::encodeStatement(f.s));writeNew(dir/(name+".proof.bin"),v);};
    auto v=raw;v[0]^=1;rawcase("wrong-magic",v);v=raw;v[4]=2;rawcase("wrong-version",v);
    v=raw;v[16]^=1;rawcase("wrong-transcript-domain",v);v=raw;v.pop_back();rawcase("truncated",v);v=raw;v.push_back(0);rawcase("trailing",v);
    v=raw;auto off=16+f.p.domain.size();std::fill(v.begin()+off,v.begin()+off+32,255);rawcase("invalid-G1",v);v=raw;std::fill(v.end()-32,v.end(),255);rawcase("noncanonical-Fr",v);
    v=raw;std::fill(v.begin()+8,v.begin()+12,255);rawcase("oversized-n",v);rawcase("oversized-message",cy::Bytes(4097,0));
    auto sr=cy::encodeStatement(f.s);sr[16]^=1;writeNew(dir/"wrong-vector-domain.statement.bin",sr);writeNew(dir/"wrong-vector-domain.proof.bin",raw);
    sr=cy::encodeStatement(f.s);sr[16+f.s.vector_domain.size()+4]^=1;writeNew(dir/"wrong-scalar-domain.statement.bin",sr);writeNew(dir/"wrong-scalar-domain.proof.bin",raw);
    std::cout<<"EMITTED public statements/proofs; no witness, value, blind or nonce files\n";
}
void benchmark() {
    using clock=std::chrono::steady_clock;
    std::cout<<"BENCH_SCOPE excludes setup/codec/startup; includes pp validation, local checks, randomness, FS, group equations\n";
    for(std::size_t n:{2u,4u}){
        auto f=fixture(n);zkproto::Statement old;old.commitment=f.s.commitment;old.coefficients=f.s.coefficients;old.value=manualDot(f.s.coefficients,f.w.values);
        constexpr int rounds=100;double cp=0,cv=0,op=0,ov=0;std::size_t cb=0,ob=0;
        auto runCommitted=[&]{auto a=clock::now();auto p=cy::prove(f.pp,f.s,f.w,f.eta);auto b=clock::now();check(cy::verify(f.pp,f.s,p),"benchmark committed rejected");auto c=clock::now();cp+=std::chrono::duration<double,std::micro>(b-a).count();cv+=std::chrono::duration<double,std::micro>(c-b).count();cb=cy::encodeProof(p).size();};
        auto runOld=[&]{auto a=clock::now();auto p=zkproto::proveLinearOpening(f.pp.vector_parameters,old,f.w);auto b=clock::now();check(zkproto::verifyLinearOpening(f.pp.vector_parameters,old,p),"benchmark public rejected");auto c=clock::now();op+=std::chrono::duration<double,std::micro>(b-a).count();ov+=std::chrono::duration<double,std::micro>(c-b).count();ob=zkproto::encodeProof(p).size();};
        // Alternate order to reduce systematic drift; same process/build/witness.
        for(int i=0;i<rounds;i++){if(i%2){runCommitted();runOld();}else{runOld();runCommitted();}}
        std::cout<<"BENCH protocol=public-y n="<<n<<" rounds="<<rounds<<" prove_us_mean="<<op/rounds<<" verify_us_mean="<<ov/rounds<<" proof_bytes="<<ob<<" statement_bytes="<<zkproto::encodeStatement(old).size()<<'\n';
        std::cout<<"BENCH protocol=committed-y n="<<n<<" rounds="<<rounds<<" prove_us_mean="<<cp/rounds<<" verify_us_mean="<<cv/rounds<<" proof_bytes="<<cb<<" statement_bytes="<<cy::encodeStatement(f.s).size()<<'\n';
    }
}
}
int main(int argc,char** argv) {
    try {
        mcl::bn::initPairing(mcl::BN254);
        if(argc==2&&std::string(argv[1])=="--red"){redTests();}
        else if(argc==2&&std::string(argv[1])=="--benchmark"){benchmark();return 0;}
        else if(argc==3&&std::string(argv[1])=="--emit"){emit(argv[2]);return 0;}
        else if(argc==1){units();}
        else {std::cout<<"usage: zk_committed_opening_tests [--red|--benchmark|--emit DIR]\n";return 2;}
        std::cout<<"RESULT passed="<<passed<<" failed="<<failed<<'\n';return failed?1:0;
    }catch(const std::exception& e){std::cout<<"FAIL "<<e.what()<<'\n';return 1;}
}
