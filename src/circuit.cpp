#include "circuit.h"
#include "utils.hpp"
#include <stdexcept>

void layeredCircuit::initSubset(bool sparse) 
{
    vector<int> visited_uidx(sparse?0:circuit[0].size);  // whether the i-th layer, j-th gate has been visited in the current layer
    vector<u64> subset_uidx(sparse?0:circuit[0].size);   // the subset index of the i-th layer, j-th gate
    vector<int> visited_vidx(sparse?0:circuit[0].size);  // whether the i-th layer, j-th gate has been visited in the current layer
    vector<u64> subset_vidx(sparse?0:circuit[0].size);   // the subset index of the i-th layer, j-th gate
    for (u32 i = 1; i < size; ++i) 
    {
        auto &cur = circuit[i], &lst = circuit[i - 1];
        bool has_pre_layer_u = circuit[i].ty == layerType::FFT || circuit[i].ty == layerType::IFFT;
        bool has_pre_layer_v = false;
        std::unordered_map<u32,u64> sparse_u,sparse_v;
        auto mapped=[&](u32 index,bool v){
            if(index>=circuit[0].size)throw std::out_of_range("subset master coordinate");
            auto &ids=v?cur.ori_id_v:cur.ori_id_u;auto &count=v?cur.size_v[0]:cur.size_u[0];
            if(sparse){auto &m=v?sparse_v:sparse_u;auto it=m.find(index);if(it!=m.end())return it->second;
                auto value=u64(count++);ids.push_back(index);m.emplace(index,value);return value;}
            auto &visited=v?visited_vidx:visited_uidx;auto &subset=v?subset_vidx:subset_uidx;
            if(visited[index]!=i){visited[index]=i;subset[index]=count++;ids.push_back(index);}return subset[index];
        };
        for (auto &gate: cur.uni_gates) 
        {
            if (!gate.lu) 
            {
                gate.u = mapped(gate.u,false);
            }
            has_pre_layer_u |= (gate.lu != 0);
        }
        for (auto &gate: cur.bin_gates) 
        {
            if (!gate.getLayerIdU(i)) 
            {
                gate.u = mapped(gate.u,false);
            }
            if (!gate.getLayerIdV(i)) {
                gate.v = mapped(gate.v,true);
            }
            has_pre_layer_u |= (gate.getLayerIdU(i) != 0);
            has_pre_layer_v |= (gate.getLayerIdV(i) != 0);
        }
        cur.bit_length_u[0] = ceilPow2BitLength(cur.size_u[0]);
        cur.bit_length_v[0] = ceilPow2BitLength(cur.size_v[0]);

        if (has_pre_layer_u) 
            switch (cur.ty) {
                case layerType::FFT:
                    cur.size_u[1] = 1ULL << cur.fft_bit_length - 1;
                    cur.bit_length_u[1] = cur.fft_bit_length - 1;
                    break;
                case layerType::IFFT:
                    cur.size_u[1] = 1ULL << cur.fft_bit_length;
                    cur.bit_length_u[1] = cur.fft_bit_length;
                    break;
                default:
                    cur.size_u[1] = lst.size ;
                    cur.bit_length_u[1] = lst.bit_length;
                    break;
            } else {
            cur.size_u[1] = 0;
            cur.bit_length_u[1] = -1;
        }
        if (has_pre_layer_v) 
        {
            if (cur.ty == layerType::DOT_PROD) {
                cur.size_v[1] = lst.size >> cur.fft_bit_length;
                cur.bit_length_v[1] = lst.bit_length - cur.fft_bit_length;
            } else {
                cur.size_v[1] = lst.size;
                cur.bit_length_v[1] = lst.bit_length;
            }
        } 
        else 
        {
            cur.size_v[1] = 0;
            cur.bit_length_v[1] = -1;
        }
        cur.updateSize();
    }
}

void layeredCircuit::init(u32 q_bit_size, u32 _layer_sz) 
{
    if (_layer_sz == 0) {
        throw std::invalid_argument("Circuit must contain at least one layer");
    }
    size = _layer_sz;
    circuit.resize(size);
}
