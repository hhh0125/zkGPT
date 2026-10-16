#include "zk_linear_opening.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <set>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>

using namespace zkproto;
namespace {
int passed = 0, failed = 0;
void check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
template<class F> void test(const std::string& name, F fn) {
    try { fn(); ++passed; std::cout << "PASS " << name << '\n'; }
    catch (const std::exception& e) {
        ++failed; std::cout << "FAIL " << name << ": " << e.what() << '\n';
    }
}
template<class F> void throws(F fn) {
    bool rejected = false;
    try { fn(); } catch (const std::exception&) { rejected = true; }
    check(rejected, "expected input rejection");
}
Fr manualDot(const std::vector<Fr>& a, const std::vector<Fr>& b) {
    Fr x=0; for (std::size_t i=0;i<a.size();++i) x+=a[i]*b.at(i); return x;
}
std::vector<Fr> randomFields(std::size_t n) {
    std::vector<Fr> a(n,Fr(0)); for(auto& x:a)x=sampleUniformScalar(); return a;
}
template<class F> void expectEntropyFailure(F fn) {
    pid_t pid=fork();
    check(pid>=0,"fork failed");
    if(pid==0) {
        sock_filter filter[]={
            BPF_STMT(BPF_LD|BPF_W|BPF_ABS,offsetof(seccomp_data,nr)),
            BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,SYS_getrandom,0,1),
            BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_ERRNO|EIO),
            BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_ALLOW)};
        sock_fprog program{static_cast<unsigned short>(sizeof(filter)/sizeof(filter[0])),filter};
        if(prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)!=0 ||
           prctl(PR_SET_SECCOMP,SECCOMP_MODE_FILTER,&program)!=0)_exit(77);
        try {fn();_exit(1);}catch(const std::exception&){_exit(0);}
    }
    int status=0;
    check(waitpid(pid,&status,0)==pid,"waitpid failed");
    check(WIFEXITED(status) && WEXITSTATUS(status)==0,
          "entropy failure did not fail closed (or seccomp unavailable)");
}
void samplerTests() {
    test("sampler_accept_zero",[]{
        std::array<std::uint8_t,32>b{};Fr x=1;
        check(detail::scalarCandidate(b,x)&&x.isZero(),"zero candidate rejected");
    });
    test("sampler_accept_modulus_minus_one",[]{
        std::array<std::uint8_t,32>b{};Fr last=-1,x=0;last.serialize(b.data(),b.size());
        check(detail::scalarCandidate(b,x)&&x==last,"modulus-1 rejected");
    });
    test("sampler_reject_exact_modulus",[]{
        std::array<std::uint8_t,32>b{};Fr last=-1,x=0;last.serialize(b.data(),b.size());
        for(auto& v:b){if(++v!=0)break;}
        check(!detail::scalarCandidate(b,x),"modulus accepted by biased masking");
    });
    test("sampler_reject_full_ff",[]{
        std::array<std::uint8_t,32>b{};b.fill(255);Fr x=0;
        check(!detail::scalarCandidate(b,x),"noncanonical entropy accepted");
    });
    test("system_entropy_failure_commitment",[]{
        auto pp=makeParameters(4);
        expectEntropyFailure([&]{randomCommit(pp,std::vector<Fr>(4,Fr(1)));});
    });
}
struct Fixture { Parameters pp; Statement s; SecretWitness w; LinearOpeningProof p; };
Fixture fixture(std::size_t n, bool zero_b=false, bool zero_w=false) {
    Fixture f; f.pp=makeParameters(n);
    std::vector<Fr> w=n==2 ? std::vector<Fr>{Fr(3),Fr(-5)}
                           : std::vector<Fr>{Fr(7),Fr(-2),Fr(0),Fr(9)};
    if(zero_w)std::fill(w.begin(),w.end(),Fr(0));
    auto c=randomCommit(f.pp,w); f.w=c.witness; f.s.commitment=c.point;
    f.s.coefficients=n==2 ? std::vector<Fr>{Fr(2),Fr(4)}
                           : std::vector<Fr>{Fr(1),Fr(0),Fr(3),Fr(-4)};
    if(zero_b)std::fill(f.s.coefficients.begin(),f.s.coefficients.end(),Fr(0));
    f.s.value=(zero_b||zero_w)?Fr(0):(n==2?Fr(-14):Fr(-29));
    f.p=proveLinearOpening(f.pp,f.s,f.w); return f;
}
void reject(const Fixture& f, const Statement& s, const LinearOpeningProof& p) {
    std::string e;check(!verifyLinearOpening(f.pp,s,p,&e),"invalid proof accepted");
    check(!e.empty(),"missing verifier rejection diagnostic");
}
void writeNew(const std::filesystem::path& p,const std::vector<std::uint8_t>& b) {
    int fd=::open(p.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
    if(fd<0)throw std::runtime_error("artifact creation failed or exists");
    std::size_t pos=0;
    while(pos<b.size()) {
        auto r=::write(fd,b.data()+pos,b.size()-pos);
        if(r<=0){::close(fd);throw std::runtime_error("artifact write failed");}
        pos+=static_cast<std::size_t>(r);
    }
    if(::close(fd)!=0)throw std::runtime_error("artifact close failed");
}
std::vector<std::uint8_t> readBounded(const std::filesystem::path& p) {
    auto n=std::filesystem::file_size(p);
    if(n>4096)throw std::runtime_error("artifact size exceeds prototype limit");
    std::ifstream f(p,std::ios::binary);
    if(!f)throw std::runtime_error("artifact read failed");
    std::vector<std::uint8_t>b(n);f.read(reinterpret_cast<char*>(b.data()),n);
    if(!f && n)throw std::runtime_error("artifact short read");
    return b;
}
void commitmentTests() {
    for(std::size_t n:{2u,4u}) {
        test("commitment_randomized_n"+std::to_string(n),[=]{
            auto pp=makeParameters(n);auto w=randomFields(n);
            auto a=randomCommit(pp,w),b=randomCommit(pp,w);
            check(a.point==commit(pp,w,a.witness.blind),"commitment equation");
            check(a.point!=b.point,"fresh blind collision");
            check(a.witness.blind!=b.witness.blind,"blind repeated");
        });
    }
    test("commitment_zero_and_nonzero_fresh_blinds",[]{
        auto pp=makeParameters(4);std::vector<Fr>w(4,Fr(0));
        auto a=randomCommit(pp,w);w[0]=Fr(1);auto b=randomCommit(pp,w);
        check(a.point!=b.point,"random commitment collision");
    });
    test("commitment_reject_lengths",[]{
        for(std::size_t n:{0u,1u,3u,8u})throws([=]{makeParameters(n);});
        auto pp=makeParameters(4);throws([&]{commit(pp,{Fr(1),Fr(2)},Fr(3));});
        throws([]{innerProduct({Fr(1)},{Fr(1),Fr(2)});});
    });
    test("commitment_reject_untrusted_parameters",[]{
        auto pp=makeParameters(4);pp.domain="zkGPT/main";
        throws([&]{commit(pp,std::vector<Fr>(4,Fr(0)),Fr(1));});
        pp=makeParameters(4);pp.hiding_generator=pp.generators[0];
        throws([&]{commit(pp,std::vector<Fr>(4,Fr(0)),Fr(1));});
    });
}
void openingTests() {
    for(std::size_t n:{2u,4u})test("opening_positive_n"+std::to_string(n),[=]{
        auto f=fixture(n);check(verifyLinearOpening(f.pp,f.s,f.p),"positive rejected");
    });
    test("opening_zero_witness",[]{auto f=fixture(4,false,true);check(verifyLinearOpening(f.pp,f.s,f.p),"zero rejected");});
    test("opening_negative_integer_and_zero_coefficients",[]{
        auto f=fixture(4);check(manualDot(f.w.values,f.s.coefficients)==Fr(-29),"literal fixture");
        check(verifyLinearOpening(f.pp,f.s,f.p),"negative field encoding rejected");
    });
    test("opening_random_coefficients",[]{
        auto f=fixture(4);f.s.coefficients=randomFields(4);
        f.s.value=manualDot(f.w.values,f.s.coefficients);
        auto p=proveLinearOpening(f.pp,f.s,f.w);
        check(verifyLinearOpening(f.pp,f.s,p),"random coefficient proof rejected");
    });
    test("privacy_b_zero_y_zero_masked_responses",[]{
        auto f=fixture(4,true);auto p=proveLinearOpening(f.pp,f.s,f.w);
        check(verifyLinearOpening(f.pp,f.s,f.p)&&verifyLinearOpening(f.pp,f.s,p),"zero b rejected");
        check(f.p.nonce_inner_product.isZero(),"zero coefficient nonce relation");
        check(f.p.nonce_commitment!=p.nonce_commitment,"nonce reused");
        auto c=openingChallenge(f.pp,f.s,f.p);
        std::vector<Fr> candidate=f.p.responses;
        for(auto& x:candidate)x/=c;
        check(candidate!=f.w.values,"old unmasked response extraction recovered witness");
        check(f.p.responses!=f.w.values,"raw witness sent");
    });
    test("opening_roundtrip_canonical",[]{
        auto f=fixture(4);auto s=encodeStatement(f.s),p=encodeProof(f.p);
        auto sd=decodeStatement(s);auto pd=decodeProof(p);
        check(encodeStatement(sd)==s&&encodeProof(pd)==p,"noncanonical roundtrip");
        check(verifyLinearOpening(f.pp,sd,pd),"decoded proof rejected");
    });
    auto f=fixture(4);
    test("system_entropy_failure_opening_masks",[&]{
        expectEntropyFailure([&]{proveLinearOpening(f.pp,f.s,f.w);});
    });
    test("crypto_reject_public_y",[&]{auto s=f.s;s.value+=Fr(1);reject(f,s,f.p);});
    test("crypto_reject_commitment",[&]{auto s=f.s;s.commitment+=f.pp.generators[0];reject(f,s,f.p);});
    test("crypto_reject_coefficients",[&]{auto s=f.s;s.coefficients[0]+=Fr(1);reject(f,s,f.p);});
    test("crypto_reject_response",[&]{auto p=f.p;p.responses[0]+=Fr(1);reject(f,f.s,p);});
    test("crypto_reject_nonce_commitment",[&]{auto p=f.p;p.nonce_commitment+=f.pp.generators[0];reject(f,f.s,p);});
    test("crypto_reject_nonce_linear_value",[&]{auto p=f.p;p.nonce_inner_product+=Fr(1);reject(f,f.s,p);});
    test("crypto_reject_blinding_consistency",[&]{auto p=f.p;p.blind_response+=Fr(1);reject(f,f.s,p);});
    test("crypto_reject_mixed_witness_material",[&]{
        auto other=fixture(4);other.w.values[0]+=Fr(1);
        auto c=randomCommit(other.pp,other.w.values);other.s.commitment=c.point;
        other.s.value=manualDot(other.w.values,other.s.coefficients);
        auto p=proveLinearOpening(other.pp,other.s,c.witness);reject(f,f.s,p);
        p=f.p;p.responses=other.p.responses;reject(f,f.s,p);
    });
    test("verifier_reject_generator_domain",[&]{auto pp=f.pp;pp.domain="wrong";check(!verifyLinearOpening(pp,f.s,f.p),"wrong pp domain accepted");});
    test("verifier_reject_generator_point",[&]{auto pp=f.pp;pp.generators[0]=pp.hiding_generator;check(!verifyLinearOpening(pp,f.s,f.p),"supplied generators trusted");});
    test("verifier_reject_transcript_domain",[&]{auto p=f.p;p.domain="wrong";reject(f,f.s,p);});
    test("verifier_reject_version",[&]{auto p=f.p;p.version=3;reject(f,f.s,p);});
    test("verifier_reject_zero_b_nonzero_y",[]{auto f=fixture(4,true);auto s=f.s;s.value=Fr(1);reject(f,s,f.p);});
    test("local_prover_reject_wrong_y",[&]{auto s=f.s;s.value+=Fr(1);throws([&]{proveLinearOpening(f.pp,s,f.w);});});
    test("local_prover_reject_wrong_blind",[&]{auto w=f.w;w.blind+=Fr(1);throws([&]{proveLinearOpening(f.pp,f.s,w);});});
    test("local_prover_reject_wrong_witness_length",[&]{auto w=f.w;w.values.pop_back();throws([&]{proveLinearOpening(f.pp,f.s,w);});});
    test("decoder_reject_truncated",[&]{auto p=encodeProof(f.p);p.pop_back();throws([&]{decodeProof(p);});});
    test("decoder_reject_trailing",[&]{auto p=encodeProof(f.p);p.push_back(0);throws([&]{decodeProof(p);});});
    test("decoder_reject_magic_byte_mutation",[&]{auto p=encodeProof(f.p);p[0]^=1;throws([&]{decodeProof(p);});});
    test("decoder_reject_noncanonical_scalar",[&]{auto p=encodeProof(f.p);std::fill(p.end()-Fr::getByteSize(),p.end(),255);throws([&]{decodeProof(p);});});
    test("decoder_reject_invalid_G1",[&]{
        auto p=encodeProof(f.p);std::size_t off=16+f.p.domain.size();
        std::fill(p.begin()+off,p.begin()+off+G1::getSerializedByteSize(),255);
        throws([&]{decodeProof(p);});
    });
    test("decoder_reject_oversized_length",[&]{auto p=encodeProof(f.p);for(std::size_t i=8;i<12;i++)p[i]=255;throws([&]{decodeProof(p);});});
    test("decoder_reject_statement_trailing",[&]{auto s=encodeStatement(f.s);s.push_back(0);throws([&]{decodeStatement(s);});});
    test("decoder_reject_statement_scalar",[&]{auto s=encodeStatement(f.s);std::fill(s.end()-Fr::getByteSize(),s.end(),255);throws([&]{decodeStatement(s);});});
    test("algebraic_special_soundness_and_simulator",[&]{
        auto d=randomFields(4);Fr r=sampleUniformScalar();Fr c=7,cp=11;
        auto A=commit(f.pp,d,r);auto af=manualDot(d,f.s.coefficients);
        auto z=d,zp=d;
        for(std::size_t i=0;i<4;i++){z[i]+=c*f.w.values[i];zp[i]+=cp*f.w.values[i];}
        Fr zr=r+c*f.w.blind,zrp=r+cp*f.w.blind;
        check(commit(f.pp,z,zr)==A+f.s.commitment*c,"first Sigma group equation");
        check(manualDot(z,f.s.coefficients)==af+c*f.s.value,"first Sigma scalar equation");
        check(commit(f.pp,zp,zrp)==A+f.s.commitment*cp,"second Sigma group equation");
        std::vector<Fr> ex(4,Fr(0));for(std::size_t i=0;i<4;i++)ex[i]=(z[i]-zp[i])/(c-cp);
        check(ex==f.w.values && (zr-zrp)/(c-cp)==f.w.blind,"two-transcript extraction");
        auto simulated=randomFields(4);Fr sr=sampleUniformScalar();
        auto sa=commit(f.pp,simulated,sr)-f.s.commitment*c;
        auto saf=manualDot(simulated,f.s.coefficients)-c*f.s.value;
        check(commit(f.pp,simulated,sr)==sa+f.s.commitment*c,"simulator group equation");
        check(manualDot(simulated,f.s.coefficients)==saf+c*f.s.value,"simulator scalar equation");
    });
}
void emitFixtures(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);
    auto emit=[&](const std::string& name,const Statement& s,const LinearOpeningProof& p){
        writeNew(dir/(name+".statement.bin"),encodeStatement(s));
        writeNew(dir/(name+".proof.bin"),encodeProof(p));
    };
    auto f=fixture(4);emit("positive-n4",f.s,f.p);
    auto n2=fixture(2);emit("positive-n2",n2.s,n2.p);
    auto b0=fixture(4,true);emit("positive-b0",b0.s,b0.p);
    auto s=f.s;s.value+=Fr(1);emit("wrong-y",s,f.p);
    s=f.s;s.commitment+=f.pp.generators[0];emit("wrong-C",s,f.p);
    s=f.s;s.coefficients[0]+=Fr(1);emit("wrong-b",s,f.p);
    auto p=f.p;p.responses[0]+=Fr(1);emit("wrong-response",f.s,p);
    p=f.p;p.blind_response+=Fr(1);emit("wrong-blind-response",f.s,p);
    auto other=fixture(4);emit("mixed-proof",f.s,other.p);
    p=f.p;p.domain="wrong";emit("wrong-transcript-domain",f.s,p);
    s=f.s;s.generator_domain="wrong";emit("wrong-generator-domain",s,f.p);
    auto raw=encodeProof(f.p);
    auto rawcase=[&](const std::string& name,const std::vector<std::uint8_t>& bytes){
        writeNew(dir/(name+".statement.bin"),encodeStatement(f.s));writeNew(dir/(name+".proof.bin"),bytes);
    };
    auto corrupt=raw;corrupt[0]^=1;rawcase("mutated-magic",corrupt);
    corrupt=raw;corrupt.pop_back();rawcase("truncated",corrupt);
    corrupt=raw;corrupt.push_back(0);rawcase("trailing",corrupt);
    corrupt=raw;std::size_t off=16+f.p.domain.size();
    std::fill(corrupt.begin()+off,corrupt.begin()+off+G1::getSerializedByteSize(),255);rawcase("invalid-G1",corrupt);
    corrupt=raw;std::fill(corrupt.end()-Fr::getByteSize(),corrupt.end(),255);rawcase("noncanonical-Fr",corrupt);
    std::cout<<"EMITTED public-only fixtures; no witness or blind files\n";
}
void benchmark() {
    using clock=std::chrono::steady_clock;
    for(std::size_t n:{2u,4u}) {
        auto f=fixture(n);constexpr int rounds=100;double pt=0,vt=0;
        std::size_t bytes=0;
        for(int i=0;i<rounds;i++) {
            auto a=clock::now();auto p=proveLinearOpening(f.pp,f.s,f.w);auto b=clock::now();
            check(verifyLinearOpening(f.pp,f.s,p),"benchmark rejected");auto c=clock::now();
            pt+=std::chrono::duration<double,std::micro>(b-a).count();
            vt+=std::chrono::duration<double,std::micro>(c-b).count();bytes=encodeProof(p).size();
        }
        std::cout<<"BENCH n="<<n<<" rounds="<<rounds<<" prove_us_mean="<<pt/rounds
                 <<" verify_us_mean="<<vt/rounds<<" proof_bytes="<<bytes
                 <<" statement_bytes="<<encodeStatement(f.s).size()<<'\n';
    }
}
}
int main(int argc,char** argv) {
    try {
        mcl::bn::initPairing(mcl::BN254);
        if(argc==4 && std::string(argv[1])=="--verify") {
            Statement s;LinearOpeningProof p;
            try{s=decodeStatement(readBounded(argv[2]));p=decodeProof(readBounded(argv[3]));}
            catch(const std::exception& e){std::cout<<"DECODE_REJECT "<<e.what()<<'\n';return 2;}
            std::string e;
            if(!verifyLinearOpening(makeParameters(s.coefficients.size()),s,p,&e)){
                std::cout<<"CRYPTO_REJECT "<<e<<'\n';return 1;
            }
            std::cout<<"ACCEPT public statement and ZK linear opening\n";return 0;
        }
        if(argc==3&&std::string(argv[1])=="--emit"){emitFixtures(argv[2]);return 0;}
        if(argc==2&&std::string(argv[1])=="--benchmark"){benchmark();return 0;}
        if(argc==2&&std::string(argv[1])=="--commitment"){commitmentTests();}
        else if(argc==2&&std::string(argv[1])=="--opening"){openingTests();}
        else if(argc==2&&std::string(argv[1])=="--sampler"){samplerTests();}
        else if(argc==1){samplerTests();commitmentTests();openingTests();}
        else {std::cerr<<"usage: zk_opening_tests [--commitment|--opening|--benchmark|--emit DIR|--verify STATEMENT PROOF]\n";return 2;}
        std::cout<<"RESULT passed="<<passed<<" failed="<<failed<<'\n';return failed?1:0;
    }catch(const std::exception& e){std::cout<<"FAIL "<<e.what()<<'\n';return 1;}
}
