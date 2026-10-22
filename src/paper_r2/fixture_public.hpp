#pragma once
#include "circuit.h"
#include "utils.hpp"
// Public wiring for the existing four-FCONN regression fixture; no witness,
// model files, prover or private state is constructed by the verifier.
inline layeredCircuit r2FixtureCircuit(){
    layeredCircuit c;c.init(0,9);initLayer(c.circuit[0],32,layerType::INPUT);
    for(unsigned f=0;f<4;++f){
        auto &m=c.circuit[1+2*f];initLayer(m,4,layerType::FCONN);m.need_phase2=true;
        for(unsigned i=0;i<4;++i)m.ori_id_u.push_back(i);
        for(unsigned i=0;i<16;++i)m.ori_id_v.push_back(4+i);
        m.size_u[0]=4;m.size_v[0]=16;
        auto &p=c.circuit[2+2*f];initLayer(p,4,layerType::Sqr);p.need_phase2=true;
        for(unsigned i=0;i<4;++i)p.bin_gates.emplace_back(i,i,i,1,1);
    }
    c.initSubset();return c;
}
