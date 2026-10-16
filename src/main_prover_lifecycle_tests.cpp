#include "tests/gkr_test_fixture.hpp"
#include "models.hpp"
#include <atomic>
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#include <new>
#include <iostream>
#include <stdexcept>
namespace {
struct Arrays { std::mutex mutex; std::unordered_map<void*,std::size_t> live; };
Arrays &arrays() { static Arrays *state=new Arrays; return *state; }
std::atomic<int> fail_after{-1};
bool live(void *p) { auto &s=arrays(); std::lock_guard<std::mutex> lock(s.mutex); return s.live.count(p); }
void require(bool value,const char *message) { if (!value) throw std::runtime_error(message); }
}
void *operator new[](std::size_t bytes) {
    int n=fail_after.load();
    while (n>=0) {
        if (fail_after.compare_exchange_weak(n,n-1)) { if (!n) throw std::bad_alloc(); break; }
    }
    void *p=std::malloc(bytes?bytes:1); if (!p) throw std::bad_alloc();
    try { auto &s=arrays(); std::lock_guard<std::mutex> lock(s.mutex); s.live.emplace(p,bytes); }
    catch (...) { std::free(p); throw; }
    return p;
}
void operator delete[](void *p) noexcept {
    if (!p) return;
    auto &s=arrays(); { std::lock_guard<std::mutex> lock(s.mutex); s.live.erase(p); }
    std::free(p);
}
void operator delete[](void *p,std::size_t) noexcept { operator delete[](p); }
class PartialNetwork:public LLM {
public:
    PartialNetwork():LLM(1,12,64,768,3072) {}
    void load(std::vector<std::vector<F>> &witness) {
        val=witness.begin(); channel_out=2; channel_in=2;
        readFconWeight(0,2,2,0);
    }
};
class FailedNetworkConstruction:public PartialNetwork {
public:
    explicit FailedNetworkConstruction(void *&allocation) {
        std::vector<std::vector<F>> witness(1,std::vector<F>(4));
        try { load(witness); }
        catch (...) { allocation=mat_values[0]; throw; }
        throw std::runtime_error("expected shape failure");
    }
};
int main() {
    try {
        initPairing(mcl::BN254);
        for (int instance=0;instance<3;++instance) {
            layeredCircuit circuit; GKRPublicStatement statement; GKRProof proof;
            std::vector<uint8_t> bytes; void *w=nullptr,*rows=nullptr;
            {
                gkr_test::SmallFconnFixture fixture;
                circuit=fixture.circuit(); statement=fixture.statement(); proof=fixture.proof(); bytes=fixture.proofBytes();
                w=fixture.proverInstance().cc.w; rows=fixture.proverInstance().cc.comm;
                require(live(w)&&live(rows),"commitment owners not tracked");
            }
            require(!live(w)&&!live(rows),"normal destruction retained commitment arrays");
            std::string error;
            require(verifyGKR(circuit,statement,proof,&error),"verification after destruction failed");
            require(serializeGKRProof(proof)==bytes,"proof bytes changed after destruction");
        }
        std::vector<G1> generators(4); for (auto &g:generators) hashAndMapToG1(g,"lifecycle-generator");
        for (int failure=0;failure<3;++failure) {
            std::size_t before; { auto &s=arrays(); std::lock_guard<std::mutex> lock(s.mutex); before=s.live.size(); }
            bool threw=false;
            {
                prover p; p.C.circuit.resize(1); p.C.circuit[0].size=4; p.C.circuit[0].bit_length=2; p.val.resize(1); p.val[0].resize(4); for (auto &v:p.val[0]) v=1;
                fail_after=failure;
                try { p.commitInput(generators,1); } catch (const std::bad_alloc &) { threw=true; }
                fail_after=-1;
            }
            require(threw,"injected allocation failure not propagated");
            auto &s=arrays(); std::lock_guard<std::mutex> lock(s.mutex);
            require(s.live.size()==before,"exception path retained arrays");
        }
        void *matrix=nullptr;
        {
            PartialNetwork network; std::vector<std::vector<F>> witness(1,std::vector<F>(4));
            bool threw=false;
            try { network.load(witness); } catch (const std::runtime_error &) { threw=true; }
            require(threw,"partial model load did not fail");
            matrix=network.mat_values[0]; require(live(matrix),"partial model allocation not owned");
        }
        require(!live(matrix),"partial model load retained matrix");
        matrix=nullptr;
        try { FailedNetworkConstruction network(matrix); }
        catch (const std::runtime_error &) {}
        require(matrix&&!live(matrix),"partial construction retained matrix");
        void *w=nullptr,*rows=nullptr;
        try {
            gkr_test::SmallFconnFixture fixture; w=fixture.proverInstance().cc.w; rows=fixture.proverInstance().cc.comm;
            throw std::runtime_error("after-proof injected exception");
        } catch (const std::runtime_error &) {}
        require(!live(w)&&!live(rows),"after-proof exception retained arrays");
        std::cout << "Lifecycle tests passed: repeated owners, post-destruction verification, canonical bytes, three allocation failure points, after-proof exception\n";
        return 0;
    } catch (const std::exception &e) { fail_after=-1; std::cerr<<e.what()<<'\n'; return 1; }
}
