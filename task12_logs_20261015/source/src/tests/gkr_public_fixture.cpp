#include "tests/gkr_public_fixture.hpp"
#include "utils.hpp"
namespace gkr_test {
void buildSmallFconnPublicCircuit(layeredCircuit& circuit) {
    constexpr auto dimension=kSmallFconnDimension;
    circuit.init(0,1+2*kSmallFconnCount);
    initLayer(circuit.circuit[0],32,layerType::INPUT);
    for (unsigned fc=0;fc<kSmallFconnCount;++fc) {
        auto& matrix=circuit.circuit[1+2*fc];initLayer(matrix,dimension,layerType::FCONN);
        matrix.need_phase2=true;
        for (unsigned i=0;i<dimension;++i)matrix.ori_id_u.push_back(i);
        for (unsigned i=0;i<dimension*dimension;++i)matrix.ori_id_v.push_back(dimension+i);
        matrix.size_u[0]=matrix.ori_id_u.size();matrix.size_v[0]=matrix.ori_id_v.size();
        auto& parent=circuit.circuit[2+2*fc];initLayer(parent,dimension,layerType::Sqr);
        parent.need_phase2=true;
        for (unsigned i=0;i<dimension;++i)parent.bin_gates.emplace_back(i,i,i,1,1);
    }
    circuit.initSubset();
}
}
