#include "circuit.h"
#include "utils.hpp"
#include "gkr_protocol.hpp"
#include "models.hpp"
#include <atomic>
#include <cstdlib>
#include <new>
#include <iostream>
#include <limits>
#include <type_traits>
namespace { std::atomic<bool> reject_allocations{false}; }
void *operator new(std::size_t bytes) {
    if (reject_allocations) throw std::bad_alloc();
    void *p=std::malloc(bytes?bytes:1);if (!p) throw std::bad_alloc();return p;
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p,std::size_t) noexcept { std::free(p); }
static_assert(noexcept(std::declval<layer &>().releaseEmptyUnaryStorage()),"empty release must be allocation-free noexcept");
namespace {
void check(bool condition,const char *message) { if (!condition) throw std::runtime_error(message); }
void run(std::size_t count) {
    layeredCircuit C{}; C.init(220,3);
    for (std::size_t i=0;i<C.circuit.size();++i) initLayer(C.circuit[i],8,i==0?layerType::INPUT:layerType::RELU);
    auto &input=C.circuit[0];
    input.uni_gates.reserve(count/2); // Deliberate underestimate; emplace must still grow safely.
    for (std::size_t i=0;i<count;++i) input.uni_gates.emplace_back(i%8,i%8,0,1);
    const auto before_refusal=fingerprintCircuit(C);
    const auto capacity=input.uni_gates.capacity();
    const auto pointer=input.uni_gates.data();
    if (count) {
        check(!input.releaseEmptyUnaryStorage(),"nonempty storage accepted for release");
        check(input.uni_gates.size()==count&&input.uni_gates.capacity()==capacity&&input.uni_gates.data()==pointer,"refusal changed storage");
        check(fingerprintCircuit(C)==before_refusal,"refusal changed gate order or fingerprint");
    }
    // Other layers retain nonempty unary/binary/mapping; release must not touch them.
    C.circuit[1].uni_gates.emplace_back(0,0,0,1);
    C.circuit[1].bin_gates.emplace_back(1,1,2,1,0);
    C.circuit[1].ori_id_u={0,1};C.circuit[1].ori_id_v={2};
    const auto *other=C.circuit[1].uni_gates.data();
    initLayer(input,8,layerType::INPUT); // Existing production semantic reset.
    check(input.uni_gates.empty()&&input.uni_gates.capacity()==capacity,"clear did not preserve test capacity");
    const auto before_release=fingerprintCircuit(C);
    bool released;
    reject_allocations=true;
    released=input.releaseEmptyUnaryStorage();
    reject_allocations=false;
    check(released&&input.uni_gates.empty()&&input.uni_gates.capacity()==0,"empty release retained capacity");
    check(C.circuit[1].uni_gates.data()==other,"release reallocated another layer");
    check(fingerprintCircuit(C)==before_release,"release changed final circuit fingerprint");
    check(input.releaseEmptyUnaryStorage(),"repeated release failed");
    // The layer/vector remains reusable. This is not used by the final production input path.
    input.uni_gates.emplace_back(0,0,0,1);
    check(input.uni_gates.size()==1&&!input.releaseEmptyUnaryStorage(),"post-release insertion/refusal failed");
    // Fault before a requested reserve: no arithmetic contents or capacity change.
    const auto current=fingerprintCircuit(C);const auto current_capacity=input.uni_gates.capacity();
    bool failed=false; reject_allocations=true;
    try { input.uni_gates.reserve(current_capacity+1024); } catch (const std::bad_alloc &) { failed=true; }
    reject_allocations=false;
    check(failed&&input.uni_gates.capacity()==current_capacity&&fingerprintCircuit(C)==current,"reserve failure changed circuit");
}
}
int main() {
    try {
        initPairing(mcl::BN254);
        for (auto n:{std::size_t{0},std::size_t{1},std::size_t{3},std::size_t{1024},std::size_t{1}<<20}) run(n);
        // Public builder shape variants exercise the actual production reset/release
        // path and require valid final metadata and untouched mapping geometry.
        for (int depth:{1,2}) {
            LLM network(depth,2,8,16,32);
            const auto C=network.buildPublicCircuit();
            check(C.size==1+23*depth,"shape layer count changed");
            check(C.circuit[0].uni_gates.empty()&&C.circuit[0].uni_gates.capacity()==0,"public builder retained empty input storage");
            std::size_t gates=0,mapping=0;
            for (const auto &layer:C.circuit) { gates+=layer.uni_gates.size()+layer.bin_gates.size();mapping+=layer.ori_id_u.size()+layer.ori_id_v.size(); }
            check(gates&&mapping,"shape builder dropped valid gates/mappings");
            std::cout << "CIRCUIT_VECTOR depth=" << depth << " gates=" << gates << " mappings=" << mapping << " fingerprint=" << circuitFingerprintHex(fingerprintCircuit(C)) << '\n';
        }
        std::cout << "Circuit storage tests passed: empty/single/large/multilayer/underestimate, live gate refusal, allocation failure, fingerprint invariance, shape builders\n";
        return 0;
    } catch (const std::exception &e) { reject_allocations=false;std::cerr<<e.what()<<'\n';return 1; }
}
