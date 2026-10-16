#include "lasso_folding_workspace.hpp"
#include "range_protocol.hpp"
#include "thread_joiner.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace mcl::bn;
namespace {
void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
struct Polynomial { Fr a,b,c; };
Polynomial polynomial(Fr *x, Fr *y, std::size_t length) {
    Fr A=0,B=0,C=0;
    for (std::size_t k=0;k<length;++k) {
        if (k&1) {
            B+=x[k]*y[k];
            C+=(x[k]+x[k]-x[k-1])*(y[k]+y[k]-y[k-1]);
        } else A+=x[k]*y[k];
    }
    return {(C+A)/2-B,2*B-C/2-3*A/2,A};
}
void run(int bits, std::size_t logical, int pattern, int threads) {
    const std::size_t N=std::size_t{1}<<bits;
    // Guard both boundaries of the borrowed sources and the old reference.
    std::vector<Fr> x(N+2,Fr(0)),y(N+2,Fr(0));
    x.front()=y.front()=Fr(991); x.back()=y.back()=Fr(997);
    for (std::size_t j=0;j<logical;++j) {
        x[j+1]=Fr(pattern==0 || j%5 ? 0 : static_cast<int>(j%101)-50);
        y[j+1]=Fr(pattern==0 || (pattern==1 && j%7) ? 0 : static_cast<int>(j%97)-48);
    }
    std::vector<std::vector<Fr>> oldx(bits+1),oldy(bits+1);
    oldx[0]=x; oldy[0]=y;
    zkgpt::LassoFoldingWorkspace<Fr> ws(bits);
    Transcript reference("lasso-workspace-test"),optimized("lasso-workspace-test");
    Fr random=0;
    std::size_t initialized=N;
    for (int i=0;i<=bits;++i) {
        const std::size_t length=N>>i;
        Fr *rx=oldx[i].data()+1,*ry=oldy[i].data()+1;
        Fr *nx=i ? ws.input1() : x.data()+1;
        Fr *ny=i ? ws.input2() : y.data()+1;
        const auto a=polynomial(rx,ry,length),b=polynomial(nx,ny,initialized);
        check(a.a==b.a && a.b==b.b && a.c==b.c,"per-round polynomial differs");
        if (i==bits) {
            check(rx[0]==nx[0] && ry[0]==ny[0],"final evaluation differs");
            break;
        }
        for (auto *t : {&reference,&optimized}) {
            const auto &p=t==&reference ? a : b;
            t->appendFr("lasso.sumcheck.a",p.a);
            t->appendFr("lasso.sumcheck.b",p.b);
            t->appendFr("lasso.sumcheck.c",p.c);
        }
        random=reference.challenge("lasso.sumcheck.challenge");
        check(random==optimized.challenge("lasso.sumcheck.challenge"),"challenge differs");
        const int round=i+1;
        const std::size_t out=length/2;
        oldx[round].assign(out+2,Fr(0)); oldy[round].assign(out+2,Fr(0));
        oldx[round].front()=oldy[round].front()=Fr(991);
        oldx[round].back()=oldy[round].back()=Fr(997);
        for (std::size_t k=0;k<out;++k) {
            // Test-only original implementation. Never linked to demo/prover.
            Fr &c=rx[2*k+1],&d=rx[2*k];
            oldx[round][k+1]=c==d ? d : (c-d)*random+d;
            Fr &a=ry[2*k+1],&b=ry[2*k];
            oldy[round][k+1]=a==b ? b : (a-b)*random+b;
        }
        ws.beginRound(round);
        check(ws.inputLength()==length && ws.outputLength()==out,"active bounds differ");
        check(ws.capacity()==2*(out+(round==1 ? 0 : length)),"capacity differs");
        check(ws.output1()!=nx && ws.output2()!=ny,"input/output aliases");
        std::size_t written=out;
        if (round<=9 && bits-round>=11) {
            const std::size_t chunk=out>>10;
            written=0;
            for (std::size_t j=0;j<1024;++j) {
                if ((std::size_t{1}<<round)*(chunk*j)>=logical) break;
                written=chunk*(j+1);
            }
        }
        check(2*written<=initialized,"read exceeds initialized prefix");
        const bool next_serial=round==9 || bits-round-1<11;
        if (next_serial) ws.zeroOutputSuffix(written);
        initialized=next_serial ? out : written;
        std::vector<std::thread> workers;
        zkgpt::ThreadJoiner joiner(workers);
        for (int t=0;t<threads;++t) {
            const std::size_t begin=written*t/threads,end=written*(t+1)/threads;
            workers.emplace_back([&,begin,end] {
                for (std::size_t k=begin;k<end;++k)
                    zkgpt::lassoFoldEntry(nx,ny,ws.output1(),ws.output2(),k,random);
            });
        }
        joiner.join();
        for (std::size_t k=0;k<initialized;++k) {
            check(ws.output1()[k]==oldx[round][k+1],"coefficient fold differs");
            check(ws.output2()[k]==oldy[round][k+1],"witness fold differs");
        }
        check(oldx[round].front()==Fr(991) && oldx[round].back()==Fr(997),"reference guard changed");
        ws.finishRound();
        check(ws.capacity()==2*out,"previous buffer not released");
    }
    check(x==oldx[0] && y==oldy[0],"borrowed source modified");
    ws.release(); check(ws.capacity()==0,"scratch not released");
}
}
int main() {
    initPairing(mcl::BN254);
    check(sizeof(Fr)==32,"unexpected Fr size");
    int cases=0;
    for (int n=0;n<=13;++n) {
        const std::size_t N=std::size_t{1}<<n;
        for (std::size_t logical : {std::size_t{0},std::size_t{1},N/2+1,N-1,N})
            for (int pattern=0;pattern<3;++pattern)
                for (int threads : {1,8,32}) {
                    run(n,logical,pattern,threads); ++cases;
                }
    }
    // Exercise i==9 transition while the next output would still be large,
    // and the single-element old-task boundary near n-i==10.
    for (int threads : {1,32}) {
        run(20,(std::size_t{1}<<19)+19,1,threads); ++cases;
    }
    bool rejected=false;
    try { zkgpt::LassoFoldingWorkspace<Fr> invalid(31); }
    catch (const std::invalid_argument &) { rejected=true; }
    check(rejected,"invalid domain accepted");
    zkgpt::LassoFoldingWorkspace<Fr> ws(2);
    rejected=false;
    try { ws.beginRound(2); } catch (const std::logic_error &) { rejected=true; }
    check(rejected,"invalid transition accepted");
    ws.beginRound(1); rejected=false;
    try { ws.zeroOutputSuffix(3); } catch (const std::out_of_range &) { rejected=true; }
    check(rejected,"invalid output boundary accepted");
    std::cout << "Lasso folding reference/threads/bounds/padding/polynomial/challenge: "
              << cases << " cases passed" << std::endl;
}
