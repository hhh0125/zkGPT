//
// Created by 69029 on 3/16/2021.
//

#ifndef ZKCNN_NEURALNETWORK_HPP
#define ZKCNN_NEURALNETWORK_HPP

#include <cstddef>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include "circuit.h"
#include "prover.hpp"
#include "witness_registry.hpp"

using std::vector;
using std::tuple;
using std::pair;
using std::string;

enum convType {
    FFT, NAIVE, NAIVE_FAST
};

struct convKernel {
    convType ty;
    i64 channel_out, channel_in, size, stride_bl, padding, weight_start_id, bias_start_id;
    convKernel(convType _ty, i64 _channel_out, i64 _channel_in, i64 _size, i64 _log_stride, i64 _padding) :
            ty(_ty), channel_out(_channel_out), channel_in(_channel_in), size(_size), stride_bl(_log_stride), padding(_padding) {
    }

    convKernel(convType _ty, i64 _channel_out, i64 _channel_in, i64 _size) :
            convKernel(_ty, _channel_out, _channel_in, _size, 0, _size >> 1) {
    }
};

struct fconKernel {
    i64 channel_out, channel_in, weight_start_id, bias_start_id;
    fconKernel(i64 _channel_out, i64 _channel_in):
        channel_out(_channel_out), channel_in(_channel_in) {}
};

enum poolType {
    AVG, MAX, NONE
};

enum actType {
    RELU_ACT
};

struct poolKernel {
    poolType ty;
    i64 size, stride_bl, dcmp_start_id, max_start_id, max_dcmp_start_id;
    poolKernel(poolType _ty, i64 _size, i64 _log_stride):
            ty(_ty), size(_size), stride_bl(_log_stride) {}
};


class neuralNetwork 
{
public:
    struct ParameterRegion { std::string name; std::size_t offset, count, rows, cols; };
    const std::vector<ParameterRegion>& parameterRegions() const { return parameter_regions_; }
    int headnum, headdim, attn_dim, linear_dim;
    explicit neuralNetwork(i64 psize_x, i64 psize_y, i64 pchannel, i64 pparallel, const string &i_filename,
                           const string &c_filename, const string &o_filename, bool i_llm=false,
                           int headnum=0, int headdim=0, int attn_dim=0, int linear_dim=0);

    neuralNetwork(i64 psize, i64 pchannel, i64 pparallel, i64 kernel_size, i64 sec_size, i64 fc_size,
                  i64 start_channel, poolType pool_ty);

    void setWitnessCapacityHint(std::size_t expected){
        if(!expected || expected>(std::size_t(1)<<28))throw std::invalid_argument("witness capacity hint outside supported domain");
        witness_capacity_hint_=expected;
    }
    void create(prover &pr, bool merge);
    void createFiniteWitness(prover &pr);
    // Explicit R7 Full12 witness-only entry. Never calls the legacy PC.
    // The caller must enforce its memory/CPU budget and suppress private logs.
    void createFull12FiniteWitness(prover &pr);
    // Opt-in partial engineering relation; unresolved paper semantics stay explicit.
    void setR4NumericalCandidate(bool enabled=true) { r4_numerical_candidate_=enabled; }
    void setR5BEngineeringCandidate(bool enabled=true) { r5b_numerical_candidate_=enabled; }
    void setR6EngineeringCandidate(bool enabled=true) { r6_engineering_=enabled; r5b_numerical_candidate_=enabled; }
    struct BlockConnection { std::size_t block,input,attention_residual,output,first_layer,last_layer; };
    const std::vector<BlockConnection>& blockConnections() const { return r6_connections_; }
    std::size_t metadataWitnessElements() const { return r6_metadata_witness_elements_; }
    void logPrivateMemory(const std::string &stage) const;
    layeredCircuit buildPublicCircuit();
    void validateCurrentWitness(const prover &pr) const;
    const WitnessRegistry &getWitnessRegistry() const { return witness_registry; }
    int getSequenceLength() const { return len; }
    int getLayerCount() const { return layer_num; }
    int getHeadCount() const { return headnum; }
    int getHeadDimension() const { return headdim; }
    int getHiddenDimension() const { return attn_dim; }
    int getMlpDimension() const { return linear_dim; }
    int layer_norm_w_c[30], layer_norm_w_e[30],layer_norm_b_c[30], layer_norm_b_e[30];
    int layer_norm_w_q_start[30],layer_norm_b_q_start[30];
    int layer_norm_c1[30], layer_norm_e1[30], layer_norm_c2[30], layer_norm_e2[30];
    int channel_in, channel_out;
    int layer_num;
    int* mat_values[300]{}; // Borrowed views; never delete these entries.
protected:
    std::vector<ParameterRegion> parameter_regions_;
    std::size_t witness_capacity_hint_=0;
    struct Input0Segment {
        string name;
        size_t original_scalar_count;
        size_t padded_scalar_count;
        size_t start_scalar;
    };

    void initParam(prover& pr,int d);

    int getNextBit(int layer_id);

    void calcSizeAfterPool(const poolKernel &p);

    void refreshFCParam(const fconKernel &fc);

    [[nodiscard]] i64 getFFTLen() const;

    [[nodiscard]] i8 getFFTBitLen() const;


    void prepareDecmpBit(i64 layer_id, i64 idx, i64 dcmp_id, i64 bit_shift);

    void prepareFieldBit(const F &data, i64 dcmp_id, i64 bit_shift);

    void prepareSignBit(i64 layer_id, i64 idx, i64 dcmp_id);

    void prepareMax(i64 layer_id, i64 idx, i64 max_id);

    void calcInputLayer(layer &circuit);

    void calcNormalLayer(const layer &circuit, i64 layer_id,bool output=false);
    void merge_layer(prover &pr,i64 layer_id) ;
    void checkNormalLayer(const layer &circuit, i64 layer_id,const vector<vector<F> > & vvv);

    void calcDotProdLayer(const layer &circuit, i64 layer_id);

    void calcFFTLayer(const layer &circuit, i64 layer_id);

    vector<vector<convKernel>> conv_section;
    vector<poolKernel> pool;
    poolType pool_ty;
    i64 pool_bl, pool_sz;
    i64 pool_stride_bl, pool_stride;
    i64 pool_layer_cnt, act_layer_cnt, conv_layer_cnt;
    actType act_ty;

    vector<fconKernel> full_conn;

    i64 pic_size_x, pic_size_y, pic_channel, pic_parallel;
    i64 SIZE;
    const i64 NCONV_FAST_SIZE, NCONV_SIZE, FFT_SIZE, AVE_POOL_SIZE, FC_SIZE, RELU_SIZE;
    i64 T = 0; // Diagnostic only; checked before increment in Softmax.
    const i64 Q = 9;
    i64 Q_MAX;
    const i64 Q_BIT_SIZE = 220;

    
    i64 total_in_size, total_para_size, total_relu_in_size, total_ave_in_size, total_max_in_size;
    int x_bit, w_bit, x_next_bit;

    vector<vector<F>>::iterator val;

    bool is_llm;
    bool structure_only = false;
    bool r4_numerical_candidate_ = false;
    bool r5b_numerical_candidate_ = false;
    bool r6_engineering_ = false;
    bool r6_skip_commit_ = false;
    std::size_t r6_master_size_=0, r6_metadata_witness_elements_=0;
    std::vector<BlockConnection> r6_connections_;
    bool r6Metadata() const { return r6_engineering_ && structure_only; }
    std::size_t r5bMasterSize() const { return r6Metadata()?r6_master_size_:val[0].size(); }
    int r5bAllocate(int count);
    void r5bFinish(layer &c,i64 &id);
    void r5bRound(layer &c,i64 &id,int bias_offset);
    void r5bAttention(layer *c,i64 &id);
    void r5bLayerNorm(layer *c,i64 &id,int ln_id,int real_width);
    void r5bGelu(layer *c,i64 &id,int real_width);

    int input_e,input_c;
    int q_offset;
    int block_residual_offset;
    int attention_residual_offset;
    int gelu_aux_start;
    int ln_aux_start;
    int softmax_aux_start;
    int len;
    static const int EXP_TABLE_SIZE = 655360;
    vector<int> table;
   
    int positive_check;
    int exp_check;

    void read_layer_norm(int ln_id);
    void inputLayer(layer &circuit);
    void compute_e_table();
    void addBiasLayer(layer &circuit, i64 &layer_id, i64 first_bias_id);

    void roundLayer(layer &circuit, i64 &layer_id, float scale,
                    bool* sparsity_map=NULL, int bias_offset=-1);

    // Store y = x + residual as input advice and constrain y-x-residual = 0.
    // GPT-2 additions are performed only after both operands have been exported
    // with the same quantization scale (Section 2.1 of the paper).
    void residualLayer(layer &circuit, i64 &layer_id, int residual_offset,
                       int element_count, const string &name);

    void multi_head_matrix_QK(layer &circuit, i64 &layer_id);

    void softmax_layer_1(layer &circuit, i64 &layer_id,float SQ,float SK,float Sv, float Sy);
    void softmax_layer_2(layer &circuit, i64 &layer_id,float SQ,float SK,float Sv, float Sy);
    void softmax_layer_3(layer &circuit, i64 &layer_id,float SQ,float SK,float Sv, float Sy);

    void ln_checker_layer1(layer &circuit, i64 &layer_id, int ln_id, int ey,int cy,int cn_in,bool* sparsity_map=NULL);
    
    void ln_checker_layer2(layer &circuit, i64 &layer_id, int ln_id, int ey,int cy,bool* sparsity_map=NULL);
    void ln_checker_layer3(layer &circuit, i64 &layer_id, int ln_id, int ey,int cy,int cn_in,bool* sparsity_map=NULL);

    void gelu_checker_layer1(layer &circuit, i64 &layer_id, int real_cn_out, int ea,int ca,int eb,int cb,int ec,int cc,int ed,int cd,int ex,int cx,int ey,int cy);
    void gelu_checker_layer2(layer &circuit, i64 &layer_id, int real_cn_out, int ea,int ca,int eb,int cb,int ec,int cc,int ed,int cd,int ex,int cx,int ey,int cy);
    void gelu_checker_layer3(layer &circuit, i64 &layer_id, int real_cn_out, int ea,int ca,int eb,int cb,int ec,int cc,int ed,int cd,int ex,int cx,int ey,int cy);

    void fullyConnLayer(layer &circuit, i64 &layer_id, i64 first_fc_id, int x_offset,int x_layer);

    static void printLayerInfo(const layer &circuit, i64 layer_id);

    void readBias(i64 first_bias_id);

    void readFconBias(i64 first_bias_id, int real_count, int id);

    void readFconWeight(i64 first_fc_id,int real_r,int real_c,int id);



    void printLayerValues(prover &pr);

    void printInfer(prover &pr);

    void resetInput0SegmentMap();
    void recordInput0Segment(const string &name, size_t original_scalar_count,
                             size_t padded_scalar_count, size_t start_scalar);
    void writeInput0SegmentMap() const;
    void resetScalarDumpBuffers();
    void recordGeluDelta3Scalar(__int128 value);
    void recordRoundDeltaScalar(__int128 value);
    void writeScalarDumpFiles() const;
    static string input0SegmentMapPath();
    static string sourceModuleOutputPath(const string &filename);
    void validateWitnessRegistry(const vector<F> &val0) const;
    void validateMappedCircuitWitness(const prover &pr) const;

    vector<Input0Segment> input0_segments;
    vector<uint64_t> gelu_delta3_limbs;
    vector<uint64_t> round_delta_limbs;
    // Ownership stays in the network, including partially loaded parameters.
    std::array<std::unique_ptr<int[]>,300> matrix_storage_;
    std::array<std::size_t,300> matrix_sizes_{};
    WitnessRegistry witness_registry;
    size_t rounding_witness_instance = 0;
    size_t layer_norm_witness_instance = 0;
    size_t gelu_witness_instance = 0;
    size_t softmax_witness_instance = 0;
};

#endif //ZKCNN_NEURALNETWORK_HPP
