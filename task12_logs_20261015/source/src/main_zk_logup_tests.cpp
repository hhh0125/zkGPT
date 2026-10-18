#include "range_logup.hpp"
#include "tests/zk_logup_adversary.hpp"
#include "tests/zk_logup_public_fixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
using namespace zkgpt::zk;
namespace {
int passed=0,failed=0;
void check(bool ok,const std::string& s){if(!ok)throw std::runtime_error(s);}
template<class F>void test(const std::string& name,F f){try{f();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failed;std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';}}
template<class F>void localReject(F f){bool threw=false;try{f();}catch(const std::exception& e){threw=true;std::cout<<"LOCAL_REJECT "<<e.what()<<'\n';}check(threw,"expected local reject");}
template<class F>void decodeReject(F f){bool threw=false;try{f();}catch(const std::exception& e){threw=true;std::cout<<"DECODE_REJECT "<<e.what()<<'\n';}check(threw,"expected decode reject");}
struct Fixture {
    FieldLogUpZkStatement s;MatrixWitness w;std::vector<ll> counts;
    MatrixWitnessView view()const{return {HyraxFrWitnessView(w.values.data(),w.values.size()),w.row_blinders};}
};
Fixture fixture(size_t n,const std::string& id="arbitrary-fr-table-v1",bool wrong=false){
    Fixture f;auto& s=f.s;s.query_index=2;s.actual_query_size=n>2?n-2:n;s.layer_id=3;s.circuit_id[0]=12;
    s.context=testing::fixtureContext();s.table_id=id;s.table=testing::publicFixtureTable(id);
    s.generator_domain={"zkGPT/range",static_cast<uint32_t>(matrixShape(n).columns)};
    s.padding_value=s.table[0];std::vector<Fr> q(n);f.counts.resize(s.table.size());
    for(size_t i=0;i<n;++i){size_t j=i>=s.actual_query_size?0:i%s.table.size();q[i]=s.table[j];++f.counts[j];}
    if(wrong)q[0]=Fr(1234567)/Fr(89);
    auto c=commitMatrix(makeProductionParameters(s.generator_domain),q,Layout::ColumnMajor);
    s.query=c.public_commitment;f.w=std::move(c.witness);return f;
}
std::filesystem::path out;
std::vector<std::string> manifest;
void writeFile(const std::filesystem::path& p,const Bytes& b){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());if(!f)throw std::runtime_error("write public fixture");}
void emitBytes(const std::string& name,const Bytes& sb,const Bytes& pb,int expected,const std::string& context=testing::fixtureContext()){
    if(out.empty())return;const auto stem=std::to_string(manifest.size());
    writeFile(out/(stem+".statement.bin"),sb);writeFile(out/(stem+".proof.bin"),pb);
    manifest.push_back("{\"name\":\""+name+"\",\"stem\":\""+stem+"\",\"expected_exit\":"+std::to_string(expected)+",\"expected_context\":\""+context+"\"}");
}
void emit(const std::string& name,const FieldLogUpZkStatement& s,const FieldLogUpZkProof& p,int expected,const std::string& context=testing::fixtureContext()){
    emitBytes(name,encodeFieldLogUpZkStatement(s),encodeFieldLogUpZkProof(s,p),expected,context);
}
void cryptoReject(const std::string& name,const FieldLogUpZkStatement& s,const FieldLogUpZkProof& p){
    // Require a complete canonical proof accepted by the bounded decoder.
    auto d=decodeFieldLogUpZkProof(s,encodeFieldLogUpZkProof(s,p));std::string error;
    check(!verifyFieldLogUpZk(s,d,&error),"false proof accepted");std::cout<<"CRYPTO_REJECT "<<name<<": "<<error<<'\n';emit(name,s,d,1);
}
}
int main(int argc,char** argv){
    mcl::bn::initPairing(mcl::BN254);
    if(argc==2){out=argv[1];if(!std::filesystem::create_directory(out)){std::cerr<<"fresh fixture directory required\n";return 2;}}
    for(size_t n:{1,2,8,16,32,64})test("real hidden FieldLogUp n="+std::to_string(n),[&]{
        auto f=fixture(n);ChallengeTraceRecorder pt,vt;auto start=std::chrono::steady_clock::now();
        auto p=proveFieldLogUpZk(f.s,f.view(),f.counts,1,&pt);
        auto proved=std::chrono::steady_clock::now();std::string e;
        check(verifyFieldLogUpZk(f.s,p,&e,&vt),e);auto verified=std::chrono::steady_clock::now();
        check(compareChallengeTraces(pt.entries,vt.entries,&e),e);
        check(pt.entries.front().label=="logup.beta","beta must be first challenge");
        auto bytes=encodeFieldLogUpZkProof(f.s,p);auto ds=decodeFieldLogUpZkStatement(encodeFieldLogUpZkStatement(f.s));
        check(verifyFieldLogUpZk(ds,decodeFieldLogUpZkProof(ds,bytes),&e),e);
        size_t responses=0;for(const auto& o:p.openings)responses+=o.z.size();
        check(responses==3*matrixShape(n).columns+3*matrixShape(f.s.table.size()).columns,"sqrt responses");
        std::cout<<"MEASURE n="<<n<<" table="<<f.s.table.size()<<" bytes="<<bytes.size()<<" response_Fr="<<responses
          <<" prove_ms="<<std::chrono::duration<double,std::milli>(proved-start).count()
          <<" verify_ms="<<std::chrono::duration<double,std::milli>(verified-proved).count()<<'\n';
        emit("valid-"+std::to_string(n),f.s,p,0);
        auto fresh=proveFieldLogUpZk(f.s,f.view(),f.counts,1);
        check(encodeFieldLogUpZkProof(f.s,fresh)!=bytes,"fresh randomness missing");
    });
    for(const auto& id:{"repeated-table-v1","singleton-table-v1"})test(id,[&]{
        auto f=fixture(8,id);auto p=proveFieldLogUpZk(f.s,f.view(),f.counts,1);std::string e;
        check(verifyFieldLogUpZk(f.s,p,&e),e);emit(id,f.s,p,0);
    });
    test("malicious query outside table",[]{auto f=fixture(8,"arbitrary-fr-table-v1",true);
        localReject([&]{proveFieldLogUpZk(f.s,f.view(),f.counts,1);});
        cryptoReject("false-query",f.s,testing::malformedFieldLogUp(f.s,f.view(),f.counts,testing::LogUpAttack::FalseQuery));});
    for(const auto& pair:std::vector<std::pair<std::string,testing::LogUpAttack>>{
        {"false-reciprocal",testing::LogUpAttack::Reciprocal},{"false-multiplicity",testing::LogUpAttack::Multiplicity},
        {"false-public-table-factor",testing::LogUpAttack::TableFactor},{"distinct-shared-claims",testing::LogUpAttack::DistinctSum},
        {"wrong-transcript-domain",testing::LogUpAttack::TranscriptDomain}})
        test(pair.first,[&]{auto f=fixture(8);auto p=testing::malformedFieldLogUp(f.s,f.view(),f.counts,pair.second);cryptoReject(pair.first,f.s,p);});
    auto f=fixture(16);auto p=proveFieldLogUpZk(f.s,f.view(),f.counts,1);auto pp=makeProductionParameters(f.s.generator_domain);
    test("terminal mutation",[&]{auto b=p;b.query_identity.f_terminal+=pp.U;cryptoReject("wrong-terminal",f.s,b);});
    test("mask mutation",[&]{auto b=p;b.table_identity.masked.coefficients[0]+=pp.U;cryptoReject("wrong-mask",f.s,b);});
    test("row mutation",[&]{auto b=p;b.Ca.rows[0]+=pp.U;cryptoReject("wrong-row",f.s,b);});
    test("response mutation",[&]{auto b=p;b.openings[3].z[0]+=Fr(1);cryptoReject("wrong-opening-response",f.s,b);});
    test("shared claim mutation",[&]{auto b=p;b.D_S+=pp.U;cryptoReject("wrong-shared-claim",f.s,b);});
    test("public factor response",[&]{auto b=p;b.public_table_factor.z+=Fr(1);cryptoReject("wrong-public-factor-response",f.s,b);});
    test("final binding",[&]{auto b=p;b.binding+=Fr(1);cryptoReject("wrong-final-binding",f.s,b);});
    test("splice original query commitment",[&]{auto s=f.s;s.query=fixture(16).s.query;cryptoReject("spliced-query-commitment",s,p);});
    test("context binding",[&]{auto s=f.s;s.context+="/other";std::string e;check(!verifyFieldLogUpZk(s,p,&e),"context replay");
        emit("wrong-expected-context",f.s,p,2,"other-application");emit("replayed-context-crypto",s,p,1,s.context);});
    test("generator domain binding",[&]{auto s=f.s;s.generator_domain.domain="zkGPT/other";std::string e;
        check(!verifyFieldLogUpZk(s,p,&e),"domain replay");emit("wrong-generator-domain",s,p,2);});
    test("fixed table pair binding",[&]{auto s=f.s;s.table[0]+=Fr(1);std::string e;
        check(!verifyFieldLogUpZk(s,p,&e),"false table accepted");emit("wrong-expected-table",s,p,2);});
    test("version length and dimension bounds",[&]{auto sb=encodeFieldLogUpZkStatement(f.s),pb=encodeFieldLogUpZkProof(f.s,p);
        auto bad=pb;bad[0]^=1;decodeReject([&]{decodeFieldLogUpZkProof(f.s,bad);});emitBytes("wrong-proof-version",sb,bad,2);
        bad=pb;bad.push_back(0);decodeReject([&]{decodeFieldLogUpZkProof(f.s,bad);});emitBytes("trailing-proof-byte",sb,bad,2);
        bad=pb;bad[8]^=1;decodeReject([&]{decodeFieldLogUpZkProof(f.s,bad);});emitBytes("wrong-proof-dimension",sb,bad,2);
        auto ss=sb;for(size_t i=24;i<32;++i)ss[i]=255;decodeReject([&]{decodeFieldLogUpZkStatement(ss);});emitBytes("oversized-statement-dimension",ss,pb,2);
    });
    test("borrowed row blinder mismatch local",[&]{auto bad=f;bad.w.row_blinders[0]+=Fr(1);localReject([&]{proveFieldLogUpZk(bad.s,bad.view(),bad.counts,1);});});
    test("padding metadata binding",[&]{auto s=f.s;s.actual_query_size-=1;cryptoReject("padding-metadata-replay",s,p);});
    test("zero public denominator rejects without remapping",[]{
        localReject([]{testing::checkPublicPole({Fr(-7),Fr(3)},Fr(7));});
        testing::checkPublicPole({Fr(1),Fr(3)},Fr(2));
    });
    test("zero public terminal factor equality",[]{
        // With T={1,3}, beta=2, terminal r=-3/2: beta+T~(r)=0,
        // even though both Boolean denominators are nonzero.
        const Fr beta=2,r=Fr(-3)/Fr(2);const Fr g=beta+Fr(1)+r*Fr(2);
        check(g.isZero(),"zero terminal algebra");auto pp=makeParameters(1);
        ScalarWitness w{g,sampleScalar()};auto D=scalarCommit(pp,w);
        Transcript a("zero-public-logup-factor"),b("zero-public-logup-factor");
        auto proof=proveEquality(pp,{D},{Fr(1)},g,{w},a,"logup.public-factor","zero-factor");
        check(verifyEquality(pp,{D},{Fr(1)},g,proof,b,"logup.public-factor","zero-factor"),"zero equality factor");
    });
    if(!out.empty()){std::ofstream m(out/"manifest.json");m<<"[\n";for(size_t i=0;i<manifest.size();++i)m<<manifest[i]<<(i+1<manifest.size()?",\n":"\n");m<<"]\n";}
    std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
