#pragma once

#include <vector>
#include <unordered_map>
#include <utility>
#include <stdexcept>
#include <unordered_set>
#include <iostream>
#include "global_var.hpp"
#include "hyrax.hpp"
using std::cerr;
using std::endl;
using std::vector;

struct uniGate {
    // 目标门索引，源门索引
    u32 g, u; 
    // 源门所在层
    u32 lu;
    // 缩放系数
    ll sc;
    uniGate(u32 _g, u32 _u, u32 _lu, ll _sc) :
        g(_g), u(_u), lu(_lu), sc(_sc) {
    }
};

struct binGate 
{
    // 目标门索引，源门索引u，源门索引v
    u32 g, u, v;
    u8  l;
    ll sc;
    binGate(u32 _g, u32 _u, u32 _v, ll _sc, u8 _l):
        g(_g), u(_u), v(_v), sc(_sc), l(_l) 
        {
    }
    // 获取输入u的层ID
    u32 getLayerIdU(u32 layer_id) const
    {
        if (l == 0)
            return 0U;

        if (layer_id == 0)
            throw std::out_of_range(
                "binGate::getLayerIdU: layer_id is zero"
            );

        return layer_id - 1U;
    }
    // 获取输入v的层ID
    u32 getLayerIdV(u32 layer_id) const
    {
        if ((l & 1U) == 0)
            return 0U;

        if (layer_id == 0)
            throw std::out_of_range(
                "binGate::getLayerIdV: layer_id is zero"
            );

        return layer_id - 1U;
    }
};

enum class layerType {
    INPUT, FFT, IFFT, ADD_BIAS, RELU, Sqr, OPT_AVG_POOL, MAX_POOL, AVG_POOL, DOT_PROD, PADDING, FCONN,  LAYER_NORM_1, LAYER_NORM_2 ,LAYER_NORM_3,GELU_1,GELU_2,GELU_3,MHA_QK,SOFTMAX_1,SOFTMAX_2,SOFTMAX_3,PLACE_HOLDER
};

class layer {
public:
    layerType ty;
    std::vector<pair<int,int> > uni_interval,bin_interval;
	u32 size{}, size_u[2]{}, size_v[2]{};
	i8 bit_length_u[2]{}, bit_length_v[2]{}, bit_length{};
    i8 max_bl_u{}, max_bl_v{};

    bool need_phase2;

    // bit decomp related
    u32 zero_start_id;

    std::vector<uniGate> uni_gates;
	std::vector<binGate> bin_gates;

	vector<u32> ori_id_u, ori_id_v;
    i8 fft_bit_length;

    // iFFT or avg pooling.
    //F scale;

	layer() 
    {
        bit_length_u[0] = bit_length_v[0] = -1;
        size_u[0] = size_v[0] = 0;
        bit_length_u[1] = bit_length_v[1] = -1;
        size_u[1] = size_v[1] = 0;
        need_phase2 = false;
        zero_start_id = 0;
        fft_bit_length = -1;
        //scale = F_ONE;
	}

	void updateSize() {
	    max_bl_u = std::max(bit_length_u[0], bit_length_u[1]);
	    max_bl_v = 0;
	    if (!need_phase2) return;

        max_bl_v = std::max(bit_length_v[0], bit_length_v[1]);
	}
};

class layeredCircuit {
public:
	vector<layer> circuit;  // 数组，i表示层数，circuit[i]:存储该层数据？
    u32 size;
    void init(u32 q_bit_size, u32 _layer_sz);
	void initSubset();
};

