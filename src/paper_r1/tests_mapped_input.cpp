#include "mapped_input.hpp"
#include <iostream>
#include <functional>
using namespace zkgpt::r1;
static void rejects(const std::function<void()> &f) {
    bool rejected=false; try { f(); } catch(const std::exception &) { rejected=true; }
    require(rejected,"malicious mapping/input accepted");
}
int main() { try {
    init();
    auto root=PCSpec::make("r2/master",29,32,Layout::RowMajor);
    std::vector<Fr> w(32,Fr(0)); for(unsigned i=0;i<29;++i) w[i]=Fr(int(i)-14);
    w[0]=Fr(23); w[27]=Fr(91); w[28]=Fr(1);
    auto a=PCSpec::make("r2/signed",3,4,Layout::ColumnMajor);
    auto b=PCSpec::make("r2/repeated",2,2,Layout::RowMajor);
    std::vector<std::vector<Fr>> child{{w[2]+Fr(128),w[3]+Fr(128),w[2]+Fr(128),Fr(0)}, {w[27],w[28]}};
    MappedInputSpec s{"r2/model-identity-and-public-Y",commit(root,w),
        {{commit(a,child[0]),{{2,2,Fr(128)},{2,1,Fr(128)}}},
         {commit(b,child[1]),{{27,2,Fr(0)}}}},
        {{0,Fr(23)},{27,Fr(91)},{28,Fr(1)}}};
    Fs pt("r2/mapped"), vt("r2/mapped");
    auto p=proveMappedInput(s,w,child,pt); verifyMappedInput(s,p,vt);
    require(pt.digest()==vt.digest(),"mapped transcript divergence");
    CheckedInputClaim terminal{{Fr(2),Fr(3)},Fr(0),{{1,2,Fr(0)},{7,1,Fr(0)}}};
    terminal.value=dot(weights(terminal.point),std::vector<Fr>{w[1],w[2],w[7],Fr(0)});
    Fs gt("with-GKR-terminal"),gv("with-GKR-terminal");
    auto gp=proveMappedInput(s,w,child,gt,{terminal});
    verifyMappedInput(s,gp,gv,{terminal});require(gt.digest()==gv.digest(),"direct terminal transcript");
    auto incorrect=terminal;incorrect.value+=Fr(1);
    rejects([&]{Fs t("with-GKR-terminal");verifyMappedInput(s,gp,t,{incorrect});});
    rejects([&]{Fs t("with-GKR-terminal");proveMappedInput(s,w,child,t,{incorrect});});
    for(const auto &claim:std::vector<CheckedInputClaim>{
        {{},w[0],{{0,1,Fr(0)}}},
        {{Fr(2)},w[2],{{2,1,Fr(0)},{2,1,Fr(0)}}},
        {{Fr(3)},(Fr(1)-Fr(3))*w[28],{{28,2,Fr(0)}}}}){
        Fs a("terminal-boundary"),b("terminal-boundary");
        auto p=proveMappedInput(s,w,child,a,{claim});verifyMappedInput(s,p,b,{claim});
    }
    auto invalid=terminal;invalid.spans={{31,2,Fr(0)}};
    rejects([&]{Fs t("invalid-checked");proveMappedInput(s,w,child,t,{invalid});});
    invalid=terminal;invalid.point.clear();invalid.spans={{1,2,Fr(0)}};
    rejects([&]{Fs t("invalid-checked");verifyMappedInput(s,gp,t,{invalid});});
    auto bad=p;bad.master.value+=Fr(1);
    rejects([&]{Fs t("r2/mapped");verifyMappedInput(s,bad,t);});
    auto other=s; other.public_values[1].value+=Fr(1);
    rejects([&]{Fs t("r2/mapped");verifyMappedInput(other,p,t);});
    other=s;other.context+="other-model";
    rejects([&]{Fs t("r2/mapped");verifyMappedInput(other,p,t);});
    other=s;other.children[0].spans[1].offset=4;
    rejects([&]{Fs t("r2/mapped");verifyMappedInput(other,p,t);});
    other=s;other.children[0].spans[0].bias+=Fr(1);
    rejects([&]{Fs t("r2/mapped");verifyMappedInput(other,p,t);});
    // Correct local range proof from a different witness must still fail input binding.
    auto detached=child;detached[0][0]+=Fr(1);other=s;
    other.children[0].commitment=commit(a,detached[0]);
    RangeSpec rs{"r2/range",a,8};Fs local("range");
    auto rp=proveRange(rs,other.children[0].commitment,detached[0],local);
    Fs lv("range");verifyRange(rs,other.children[0].commitment,rp,lv);
    rejects([&]{Fs t("r2/mapped");proveMappedInput(other,w,detached,t);});
    detached=child;detached[0][3]=Fr(1);other=s;
    other.children[0].commitment=commit(a,detached[0]);
    rejects([&]{Fs t("r2/mapped");proveMappedInput(other,w,detached,t);});
    auto padded=w;padded[31]=Fr(1);other=s;other.master=commit(root,padded);
    rejects([&]{Fs t("r2/mapped");proveMappedInput(other,padded,child,t);});
    other=s;auto wrong=a;wrong.role="r2/different-domain";
    other.children[0].commitment=commit(wrong,child[0]);
    rejects([&]{Fs t("r2/mapped");verifyMappedInput(other,p,t);});
    other=s;other.children[0].spans[0].offset=31;
    rejects([&]{other.validate();});
    other=s;other.children[0].spans[0].count=1;
    rejects([&]{other.validate();});
    // Nonzero witnesses pin the sparse span kernel, carries, unequal
    // dimensions, interval boundaries, and bias evaluations independently
    // through the prover's dense Sumcheck and authenticated endpoint.
    for(std::size_t count:{1,2,3,7,13,16,29}) {
        auto pc=PCSpec::make("r2/interval",count,32,Layout::ColumnMajor);
        auto interval=s;interval.children={{Commitment{pc,{}},{{0,count,Fr(5)}}}};
        if(count<29)interval.children[0].spans[0].offset=1;
        std::vector<Fr> vec(32,Fr(0));
        for(std::size_t i=0;i<count;++i)vec[i]=w[interval.children[0].spans[0].offset+i]+Fr(5);
        interval.children[0].commitment=commit(pc,vec);
        Fs a("interval"),b("interval");auto proof=proveMappedInput(interval,w,{vec},a);
        verifyMappedInput(interval,proof,b);require(a.digest()==b.digest(),"interval transcript");
    }
    std::cout<<"PASS mapped input: scattered, repeated, bias, X/Y/constants, domains and padding\n";
    return 0;
} catch(const std::exception &e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; } }
