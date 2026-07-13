//
// Created by 69029 on 3/16/2021.
//
#undef NDEBUG
#include "neuralNetwork.hpp"
#include "stats.hpp"
#include "utils.hpp"
#include "global_var.hpp"
#include "cnpy.h"
#include <polynomial.h>
#include <circuit.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cmath>
#include <sstream>
#include <stdexcept>

using std::cerr;
using std::endl;
using std::max;
using std::ifstream;
using std::ofstream;

ifstream in;
ifstream conf;
ofstream out;
namespace multi_max 
{
template<class T>
T max(T head) {
    return head;
}
template<class T, typename... Args>
T max(T head, Args... args) {
    T t = max<T>(args...);
    return (head > t)?head:t;
}
} // end of namespace

namespace {
std::string parameter_dir() {
    const char *dir = std::getenv("ZKGPT_PARAMETER_DIR");
    if (!dir) dir = std::getenv("ZKGPT_FC_WEIGHT_DIR");
    return dir ? dir : "data/gpt2_int";
}

std::string indexed_parameter_path(const char *prefix, int id,
                                   const char *suffix = ".bin") {
    std::ostringstream path;
    path << parameter_dir() << "/" << prefix << id << suffix;
    return path.str();
}

void checked_read(std::ifstream &stream, void *dst, std::streamsize bytes,
                  const std::string &path) {
    stream.read(reinterpret_cast<char*>(dst), bytes);
    if (!stream) throw std::runtime_error("Truncated parameter file: " + path);
}
} // namespace

neuralNetwork::neuralNetwork(i64 psize_x, i64 psize_y, i64 pchannel, i64 pparallel, const string &i_filename,
                             const string &c_filename, const string &o_filename, bool i_llm,
                             int headnum, int headdim, int attn_dim, int linear_dim) :
        pic_size_x(psize_x), pic_size_y(psize_y), pic_channel(pchannel), pic_parallel(pparallel),
        SIZE(0), NCONV_FAST_SIZE(1), NCONV_SIZE(2), FFT_SIZE(5),
        AVE_POOL_SIZE(1), FC_SIZE(1), RELU_SIZE(2), act_ty(RELU_ACT),
        headnum(headnum), headdim(headdim), attn_dim(attn_dim), linear_dim(linear_dim) 
    {
        is_llm=i_llm;
        in.open(i_filename);
        conf.open(c_filename);
}


void Out_group(Fr a,Fr b,Fr c)
{ 
    char flaga=' ',flagb=' ',flagc=' ';
    if (a>1e10)
    {
        flaga='-';
        a=-a;
    }
    
    if (b>1e10)
    {
        b=-b;
        flagb='-';
    }
    if (c>1e10)
    {
        flagc='-';
        c=-c;
    }
}

inline void track_val0_bits(const std::string &name, __int128 v) {
    stats::update_bitwidth(std::string("val0_") + name, v);
}

namespace {
void append_signed_int128_limbs(std::vector<uint64_t> &out, __int128 value) {
    const __uint128_t encoded = static_cast<__uint128_t>(value);
    out.push_back(static_cast<uint64_t>(encoded));
    out.push_back(static_cast<uint64_t>(encoded >> 64));
    out.push_back(0);
    out.push_back(0);
}

std::string markdown_escape_cell(const std::string &text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (char ch : text) {
        if (ch == '|') {
            escaped += "\\|";
        } else {
            escaped += ch;
        }
    }
    return escaped;
}
} // namespace

void neuralNetwork::resetInput0SegmentMap()
{
    input0_segments.clear();
}

void neuralNetwork::recordInput0Segment(const string &name, size_t original_scalar_count,
                                        size_t padded_scalar_count, size_t start_scalar)
{
    Input0Segment segment{name, original_scalar_count, padded_scalar_count, start_scalar};
    input0_segments.push_back(segment);

    const size_t end_scalar_exclusive = start_scalar + padded_scalar_count;
    const size_t padding_scalar_count =
        padded_scalar_count >= original_scalar_count ? padded_scalar_count - original_scalar_count : 0;

    if (padded_scalar_count == 0) {
        printf(
            "[INPUT0_SEGMENT] "
            "name=%s "
            "original_scalar_count=%zu "
            "padded_scalar_count=0 "
            "padding_scalar_count=0 "
            "start_scalar=%zu "
            "end_scalar_exclusive=%zu "
            "range=[%zu,%zu) "
            "last_scalar=NA\n",
            name.c_str(),
            original_scalar_count,
            start_scalar,
            start_scalar,
            start_scalar,
            start_scalar
        );
        return;
    }

    printf(
        "[INPUT0_SEGMENT] "
        "name=%s "
        "original_scalar_count=%zu "
        "padded_scalar_count=%zu "
        "padding_scalar_count=%zu "
        "start_scalar=%zu "
        "end_scalar_exclusive=%zu "
        "range=[%zu,%zu) "
        "last_scalar=%zu\n",
        name.c_str(),
        original_scalar_count,
        padded_scalar_count,
        padding_scalar_count,
        start_scalar,
        end_scalar_exclusive,
        start_scalar,
        end_scalar_exclusive,
        end_scalar_exclusive - 1
    );
}

string neuralNetwork::input0SegmentMapPath()
{
    return sourceModuleOutputPath("input0_scalar_segment_map.md");
}

string neuralNetwork::sourceModuleOutputPath(const string &filename)
{
    string source_path = __FILE__;
    const size_t slash = source_path.find_last_of("/\\");
    if (slash == string::npos) {
        return filename;
    }
    return source_path.substr(0, slash + 1) + filename;
}

void neuralNetwork::writeInput0SegmentMap() const
{
    const string path = input0SegmentMapPath();
    ofstream map_out(path);
    if (!map_out) {
        cerr << "Failed to write input[0] scalar segment map: " << path << endl;
        return;
    }

    size_t total_original_scalar_count = 0;
    size_t total_padded_scalar_count = 0;
    size_t total_padding_scalar_count = 0;
    size_t final_input0_scalar_length = 0;

    map_out << "# input[0] Scalar Segment Map\n\n";
    map_out << "- Index convention: zero-based\n";
    map_out << "- Range convention: left-closed, right-open `[start, end)`\n";
    map_out << "- Padding value: 0\n";
    map_out << "- Segment order: identical to the actual append/write order of `input[0]`\n\n";
    map_out << "| order | name | original_scalar_count | padded_scalar_count | padding_scalar_count | start_scalar | end_scalar_exclusive | range | last_scalar |\n";
    map_out << "|---:|---|---:|---:|---:|---:|---:|---|---:|\n";

    for (size_t order = 0; order < input0_segments.size(); ++order) {
        const Input0Segment &segment = input0_segments[order];
        const size_t end_scalar_exclusive = segment.start_scalar + segment.padded_scalar_count;
        const size_t padding_scalar_count =
            segment.padded_scalar_count >= segment.original_scalar_count
                ? segment.padded_scalar_count - segment.original_scalar_count
                : 0;

        total_original_scalar_count += segment.original_scalar_count;
        total_padded_scalar_count += segment.padded_scalar_count;
        total_padding_scalar_count += padding_scalar_count;
        final_input0_scalar_length = end_scalar_exclusive;

        map_out << "| " << order
                << " | " << markdown_escape_cell(segment.name)
                << " | " << segment.original_scalar_count
                << " | " << segment.padded_scalar_count
                << " | " << padding_scalar_count
                << " | " << segment.start_scalar
                << " | " << end_scalar_exclusive
                << " | `[" << segment.start_scalar << ", " << end_scalar_exclusive << ")`"
                << " | ";
        if (segment.padded_scalar_count == 0) {
            map_out << "NA";
        } else {
            map_out << end_scalar_exclusive - 1;
        }
        map_out << " |\n";
    }

    map_out << "\n## Summary\n\n";
    map_out << "- Total segment count: " << input0_segments.size() << "\n";
    map_out << "- Total original scalar count: " << total_original_scalar_count << "\n";
    map_out << "- Total padded scalar count: " << total_padded_scalar_count << "\n";
    map_out << "- Total padding scalar count: " << total_padding_scalar_count << "\n";
    map_out << "- Final input[0] scalar length: " << final_input0_scalar_length << "\n";
}

void neuralNetwork::resetScalarDumpBuffers()
{
    gelu_delta3_limbs.clear();
    round_delta_limbs.clear();
}

void neuralNetwork::recordGeluDelta3Scalar(__int128 value)
{
    append_signed_int128_limbs(gelu_delta3_limbs, value);
}

void neuralNetwork::recordRoundDeltaScalar(__int128 value)
{
    append_signed_int128_limbs(round_delta_limbs, value);
}

void neuralNetwork::writeScalarDumpFiles() const
{
    const string gelu_path = sourceModuleOutputPath("gelu_delta3.npy");
    const string round_path = sourceModuleOutputPath("round_delta.npy");

    cnpy::npy_save(
        gelu_path,
        gelu_delta3_limbs.data(),
        {gelu_delta3_limbs.size() / 4, 4},
        "w"
    );
    cnpy::npy_save(
        round_path,
        round_delta_limbs.data(),
        {round_delta_limbs.size() / 4, 4},
        "w"
    );

    cout << "Saved gelu_delta3 scalars to " << gelu_path
         << " count=" << (gelu_delta3_limbs.size() / 4) << endl;
    cout << "Saved round_delta scalars to " << round_path
         << " count=" << (round_delta_limbs.size() / 4) << endl;
}

// input:   [data]
//          [[conv_kernel || relu_conv_bit_decmp]{sec.size()}[max_pool]{if maxPool}[pool_bit_decmp]]{conv_section.size()}
//          [fc_kernel || relu_fc_bit_decmp]
// 初始化每层的参数
void neuralNetwork::initParam(prover &pr,int depth) 
{
    total_in_size = 0;
    total_para_size = 0;
    total_relu_in_size = 0;
    total_ave_in_size = 0;
    total_max_in_size = 0;
    // data
    i64 pos = 32*1024;
    for (int i = 0; i < full_conn.size(); ++i) 
    {
        auto &fc = full_conn[i];
        refreshFCParam(fc);
        // fc_kernel，2的幂次对齐
        pr.fc_real_input_row[i]=len;
        len=pr.fc_input_row[i]=1<<ceilPow2BitLength(len);
        pr.fc_real_input_col[i]=fc.channel_in;
        pr.fc_real_row[i]=fc.channel_in;
        pr.fc_input_col[i]=1<<ceilPow2BitLength(fc.channel_in);
        
        fc.channel_in=pr.fc_row[i]=1<<ceilPow2BitLength(fc.channel_in);
        pr.fc_real_col[i]=fc.channel_out;
        fc.channel_out=pr.fc_col[i]=1<<ceilPow2BitLength(fc.channel_out);
        
        fc.weight_start_id = pos;   // TODO calc FC pos
        pr.fc_start_id[i]=fc.weight_start_id;
        u32 para_size = pr.fc_row[i] * pr.fc_col[i];
        pos += para_size;
        total_para_size += para_size;
        fc.bias_start_id = pos;
        pos += fc.channel_out;
        total_para_size += fc.channel_out;
        //fprintf(stderr, "full conn  bias   weight: %11lld%11lld\n", channel_out, total_para_size);
    }
    total_in_size = pos;
    vector<string> layers={"l1","l2","l3","fcon","round","MHA_QK","softmax*V","softmax*v","soft_end","fcon2","round2","attn_residual","l1","l2","l3","fcon3","round","gelu1","g2","g3","fcon4","round","mlp_residual"};
    SIZE=1+layers.size()*depth;

}
void neuralNetwork::merge_layer(prover &pr,i64 layer_id)
{
    int cntp=0;
    for(int i=0;i<layer_id;i++)
    {
        if( (int)pr.C.circuit[i].ty==4 || (int)pr.C.circuit[i].ty==11)
        {
            ++cntp;
            layer v;
            v.ty=pr.C.circuit[i].ty;
            pr.C.circuit.push_back(v);
            swap(pr.C.circuit[i],pr.C.circuit[pr.C.circuit.size()-1]);
            vector<F> f;
            pr.val.push_back(f);
            swap(pr.val[i],pr.val[pr.val.size()-1]);
        }
    }
    vector<vector<F> >::iterator itf=pr.val.begin(); 
    int cnt2=0;
    for (vector<layer>::iterator it = pr.C.circuit.begin(); it <pr.C.circuit.end() ;) 
    {
        if ((int)it->ty==4 || (int)it->ty==11) 
        {
            if (cnt2<cntp)
            {
                it = pr.C.circuit.erase(it);
                itf=pr.val.erase(itf);
                cnt2++;
            }
            else
            {
                ++it;
                ++itf;
            }
        } 
        else 
        {
            ++it;
            ++itf;
        }
    }
    int offset[5]={0,0,0,0,0};
    int lazy_offset[5]={0,0,0,0,0};
    for(int i=4;i<layer_id;i++)
    {
        auto t=pr.C.circuit[i].ty;
        int os=0;
        if(t==layerType::MHA_QK || t==layerType::GELU_1 || t==layerType::LAYER_NORM_1)
            os=1;
        else if(t==layerType::SOFTMAX_1 || t==layerType::GELU_2 || t==layerType::LAYER_NORM_2)
            os=2;
        else if(t==layerType::SOFTMAX_2 || t==layerType::GELU_3 || t==layerType::LAYER_NORM_3)
            os=3;
        else if(t==layerType::SOFTMAX_3)
            os=4;
        else
            break;
        assert(os!=0);
        assert(pr.val[i].size()==pr.C.circuit[i].size);
        
        for(int j=0;j<pr.C.circuit[i].size;j++)
        {
            pr.val[os].emplace_back(pr.val[i][j]);
        }
    }
    for(int i=1;i<4;i++)
    {
        auto t=pr.C.circuit[i].ty;
        int os=0, os1=0;
        if(t==layerType::MHA_QK || t==layerType::GELU_1 || t==layerType::LAYER_NORM_1)
            os=1;
        else if(t==layerType::SOFTMAX_1 || t==layerType::GELU_2 || t==layerType::LAYER_NORM_2)
            os=2;
        else if(t==layerType::SOFTMAX_2 || t==layerType::GELU_3 || t==layerType::LAYER_NORM_3)
            os=3;
        else if(t==layerType::SOFTMAX_3)
            os=4;
        if(os==i)
        {
            pr.C.circuit[os].uni_interval.emplace_back(make_pair(0,pr.C.circuit[i].uni_gates.size()));
            pr.C.circuit[os].bin_interval.emplace_back(make_pair(0,pr.C.circuit[i].bin_gates.size()));
        }
    }
    for(int i=4;i<layer_id;i++)
    {
        auto t=pr.C.circuit[i].ty;
        int os=0, os1=0;
        if(t==layerType::MHA_QK || t==layerType::GELU_1 || t==layerType::LAYER_NORM_1)
            os=1;
        else if(t==layerType::SOFTMAX_1 || t==layerType::GELU_2 || t==layerType::LAYER_NORM_2)
            os=2;
        else if(t==layerType::SOFTMAX_2 || t==layerType::GELU_3 || t==layerType::LAYER_NORM_3)
            os=3;
        else if(t==layerType::SOFTMAX_3)
            os=4;
        else
            break;
        assert(os!=0);
        
        auto t2=pr.C.circuit[i-1].ty;
        if(t2==layerType::MHA_QK || t2==layerType::GELU_1 || t2==layerType::LAYER_NORM_1)
            os1=1;
        else if(t2==layerType::SOFTMAX_1 || t2==layerType::GELU_2 || t2==layerType::LAYER_NORM_2)
            os1=2;
        else if(t2==layerType::SOFTMAX_2 || t2==layerType::GELU_3 || t2==layerType::LAYER_NORM_3)
            os1=3;
        else if (t2==layerType::SOFTMAX_3)
            os1=4;
        
        assert(i!=os);
        offset[os1]=offset[os]=0;
        for(int j=1;j<i;j++)
        {
            auto tp=pr.C.circuit[j].ty;
            int tos=0;
            if(tp==layerType::MHA_QK || tp==layerType::GELU_1 || tp==layerType::LAYER_NORM_1)
                tos=1;
            else if(tp==layerType::SOFTMAX_1 || tp==layerType::GELU_2 || tp==layerType::LAYER_NORM_2)
                tos=2;
            else if(tp==layerType::SOFTMAX_2 || tp==layerType::GELU_3 || tp==layerType::LAYER_NORM_3)
                tos=3;
            else if(tp==layerType::SOFTMAX_3)
                tos=4;
            if(tos==os)
                offset[os]+=pr.C.circuit[j].size;
        }
        for(int j=1;j<i-1;j++)
        {
            auto tp=pr.C.circuit[j].ty;
            int tos=0;
            if(tp==layerType::MHA_QK || tp==layerType::GELU_1 || tp==layerType::LAYER_NORM_1)
                tos=1;
            else if(tp==layerType::SOFTMAX_1 || tp==layerType::GELU_2 || tp==layerType::LAYER_NORM_2)
                tos=2;
            else if(tp==layerType::SOFTMAX_2 || tp==layerType::GELU_3 || tp==layerType::LAYER_NORM_3)
                tos=3;
            else if(tp==layerType::SOFTMAX_3)
                tos=4;
            if(tos==os1)
                offset[os1]+=pr.C.circuit[j].size;
        }
        pr.C.circuit[os].uni_interval.emplace_back(make_pair(pr.C.circuit[os].uni_gates.size(),pr.C.circuit[os].uni_gates.size()+pr.C.circuit[i].uni_gates.size()));
        for(auto g=pr.C.circuit[i].uni_gates.begin();g!=pr.C.circuit[i].uni_gates.end();g++)
        {
            if((int)g->lu==0)
                pr.C.circuit[os].uni_gates.emplace_back(g->g+offset[os], g->u,0,g->sc);
            else
            {
                pr.C.circuit[os].uni_gates.emplace_back(g->g+offset[os], g->u+offset[os1],os1,g->sc);
                assert(os==os1+1);
                assert(g->u<pr.C.circuit[i-1].size);
                assert(g->u+offset[os1]<pr.val[os1].size());
            }
        }
        pr.C.circuit[os].bin_interval.emplace_back(make_pair(pr.C.circuit[os].bin_gates.size(),pr.C.circuit[os].bin_gates.size()+pr.C.circuit[i].bin_gates.size()));
        for(auto g=pr.C.circuit[i].bin_gates.begin();g!=pr.C.circuit[i].bin_gates.end();g++)
        {
            if((int)g->l==0)
                pr.C.circuit[os].bin_gates.emplace_back(g->g+offset[os],g->u,g->v,g->sc,g->l);
            else if((int)g->l==1)
                pr.C.circuit[os].bin_gates.emplace_back(g->g+offset[os],g->u+offset[os1],g->v+offset[os1],g->sc,g->l);
            else if((int)g->l==2)
                pr.C.circuit[os].bin_gates.emplace_back(g->g+offset[os],g->u+offset[os1],g->v,g->sc,g->l);
            assert(g->g+offset[os]<pr.val[os].size());
        }
    }
    pr.C.circuit.erase(pr.C.circuit.begin()+5,pr.C.circuit.begin()+layer_id-cntp);
    pr.val.erase(pr.val.begin()+5,pr.val.begin()+layer_id-cntp);

    
    pr.C.size=pr.C.circuit.size();
    for(int i=1;i<pr.C.size;i++)
    {
        initLayer(pr.C.circuit[i], pr.val[i].size(), pr.C.circuit[i].ty);
        if(pr.C.circuit[i].ty!=layerType::FCONN)
            checkNormalLayer(pr.C.circuit[i],i,pr.val);
    }
}
void neuralNetwork::create(prover &pr, bool merge) 
{

    // 计算指数表格
    compute_e_table();

    initParam(pr,layer_num);
    resetInput0SegmentMap();
    resetScalarDumpBuffers();
    //printf("Total layers num: %d\n", SIZE);
    // 初始化电路
    pr.C.init(Q_BIT_SIZE, SIZE);
    pr.val.resize(SIZE);
    val = pr.val.begin();
    i64 layer_id = 0;
    // 存储模型的输入数据
    inputLayer(pr.C.circuit[layer_id++]);
    for (int i = 0; i < full_conn.size(); ++i) 
    {
        auto &fc = full_conn[i];
        // 刷新全连接层参数
        refreshFCParam(fc);
        // 读取全连接层权重，随机生成
        readFconWeight(fc.weight_start_id,pr.fc_real_row[i],pr.fc_real_col[i],i);
        readFconBias(fc.bias_start_id, pr.fc_real_col[i], i);
    }
    // timer T;
    // int logn = pr.C.circuit[0].bit_length;
    // printf("before initiate circuit inputlayer lenth : 2^%d\n", logn);
    // u64 n_sqrt = 1ULL << (logn - (logn >> 1));
    // int c=0;
    // pr.gens.resize(n_sqrt);
    // cout << "pr.gens 实际长度：" << pr.gens.size() << endl;
    // G1 base=gen_gi(pr.gens.data(),n_sqrt);
    // pr.gens.push_back(base);
    // T.start();
    // pr.commitInput(pr.gens,32);  //commit weight
    // T.stop();
    // pr.proof_size+= 1<<(pr.cc.l/2);
    // cout<<"Model weight commit time: "<<T.elapse_sec()<<"s"<<endl;
    cout<<"++++++++++++++++++++++++++Start initiating circuit+++++++++++++++++++++++++++++++"<<endl;
    // 每一层：归一化--》自注意力--》归一化--》前馈网络
    //layers={"l1","l2","l3","fcon","round","MHA_QK","softmax*V","softmax*v","soft_end","fcon2","round2","l1","l2","l3","fcon3","round","gelu1","g2","g3","fcon4","round"};
    for (int i = 0; i < full_conn.size(); ++i) 
    {
        auto &fc = full_conn[i];
        refreshFCParam(fc);
        if(i==0)
        {
            q_offset=0;
        }
        // GPT-2 has two independent LayerNorms per transformer block.
        const int block_id = i / 4;
        const int ln_id = 2 * block_id + ((i % 4 == 2) ? 1 : 0);
        if (i % 4 == 0 || i % 4 == 2)
            read_layer_norm(ln_id);
        bool * sparsity=new bool[pr.fc_input_row[i]*pr.fc_input_col[i]];
        memset(sparsity,0,sizeof(bool)*pr.fc_input_row[i]*pr.fc_input_col[i]);
        int cnt=0;
        for(int j=0;j<pr.fc_real_input_row[i];j++)
        for(int k=0;k<pr.fc_real_input_col[i];k++)
        {
            ++cnt;
            sparsity[j*pr.fc_input_col[i]+k]=true;
        }
        // 归一化计算
        if(i%4==0 || i%4==2)
        {
            if (i % 4 == 0) block_residual_offset = q_offset;
            else attention_residual_offset = q_offset;
            ln_checker_layer1(pr.C.circuit[layer_id], layer_id,ln_id,input_e,input_c,pr.fc_real_input_col[0],sparsity);
            ln_checker_layer2(pr.C.circuit[layer_id], layer_id,ln_id,input_e,input_c,sparsity);
            ln_checker_layer3(pr.C.circuit[layer_id], layer_id,ln_id,input_e,input_c,pr.fc_real_input_col[0],sparsity);
        }
        pr.fc_input_id[i]=q_offset;
        // 
        fullyConnLayer(pr.C.circuit[layer_id], layer_id, fc.weight_start_id,q_offset,0);
        int cx=7,ex=-8,cy=3,ey=-8;
        float c_A,e_A,c_B=1,e_B=-10,c_C,e_C;
        c_A=input_c;
        e_A=input_e;
        // Keep block activations on one shared scale so both GPT-2 residual
        // additions are valid integer additions (paper, Section 2.1).
        c_C=input_c;
        e_C=input_e;
        // 数值范围检查
        roundLayer(pr.C.circuit[layer_id], layer_id,
                   (float)c_A*c_B/c_C*pow(2,e_A+e_B-e_C),
                   NULL, fc.bias_start_id);
        if (i % 4 == 1) {
            residualLayer(pr.C.circuit[layer_id], layer_id, block_residual_offset,
                          len * channel_out, "attention_residual");
        } else if (i % 4 == 3) {
            residualLayer(pr.C.circuit[layer_id], layer_id, attention_residual_offset,
                          len * channel_out, "mlp_residual");
        }
        // 注意力机制
        if(i%4==0)
        {
            multi_head_matrix_QK(pr.C.circuit[layer_id], layer_id);
            const float activation_scale = pow(2,e_C)*c_C;
            softmax_layer_1(pr.C.circuit[layer_id], layer_id,activation_scale,activation_scale,activation_scale,activation_scale);
            softmax_layer_2(pr.C.circuit[layer_id], layer_id,activation_scale,activation_scale,activation_scale,activation_scale);
            softmax_layer_3(pr.C.circuit[layer_id], layer_id,activation_scale,activation_scale,activation_scale,activation_scale);
        }
        // GELU激活函数，前馈网络层
        if(i%4==2)
        {
            gelu_checker_layer1(pr.C.circuit[layer_id], layer_id,pr.fc_real_col[i],-8,48, -8,217,-8, 252,-8, 615,ex,cx,ey,cy);
            gelu_checker_layer2(pr.C.circuit[layer_id], layer_id,pr.fc_real_col[i],-8,48, -8,217,-8, 252,-8, 615,ex,cx,ey,cy);
            gelu_checker_layer3(pr.C.circuit[layer_id], layer_id,pr.fc_real_col[i],-8,48, -8,217,-8, 252,-8, 615,ex,cx,ey,cy);
        }
    }
   
    if(merge)
    {
        merge_layer(pr,layer_id);
    }
    
    total_in_size += total_max_in_size + total_ave_in_size + total_relu_in_size;
    initLayer(pr.C.circuit[0], total_in_size, layerType::INPUT);
    assert(total_in_size == pr.val[0].size());
    printf("Total input size: 2^%d\n", pr.C.circuit[0].bit_length);
    printf("layer_id: %lld\n",layer_id);
    // writeInput0SegmentMap();
    // writeScalarDumpFiles();
    
    pr.C.initSubset();
    
    timer T;
    int logn = pr.C.circuit[0].bit_length;
    u64 n_sqrt = 1ULL << (logn - (logn >> 1));
    int c=0;
    pr.gens.resize(n_sqrt);
    G1 base=gen_gi(pr.gens.data(),n_sqrt);
    pr.gens.push_back(base);
    T.start();
    pr.commitInput(pr.gens,32);  //commit weight
    T.stop();
    pr.proof_size+= 1<<(pr.cc.l/2);
    cout<<"Model weight commit time: "<<T.elapse_sec()<<"s"<<endl;

    int cnt=0;
    for(int i=0;i<pr.C.size;i++)
        cnt+=pr.val[i].size();
    //printf("Total values size in circuit: %d\n", cnt);
    int bin=0,uni=0;
    // 统计门数量
    for(int i=0;i<pr.C.size;i++)
    {
        bin+=pr.C.circuit[i].bin_gates.size();
        uni+=pr.C.circuit[i].uni_gates.size();
    }
    //printf("Total gates num in circuit: bin %d uni %d\n", bin,uni);
    pr.mat_val=mat_values;

}

void neuralNetwork::inputLayer(layer &circuit) 
{
    initLayer(circuit, total_in_size, layerType::INPUT);
    printf("input size: 2^%d\n", circuit.bit_length);
    for (i64 i = 0; i < total_in_size; ++i) 
        circuit.uni_gates.emplace_back(i, 0, 0, 1);

    calcInputLayer(circuit);
}


pair<int,int> search(double scale)
{
    double mindiff=1e9;
    int best_e,best_c;
    for(int e=-10;e<=10;e++)
    for(int c=1;c<=800;c++)
    {
        double s=pow(2,e)*c;
        if(abs(s-scale)<mindiff)
        {
            mindiff=abs(s-scale);
            best_e=e;
            best_c=c;
        }
    }
    // stats::record_search(scale,best_e,best_c);
    return make_pair(best_e,best_c);
}
//为神经网络的层归一化操作准备权重和偏置参数，并将它们添加到电路的输入值中
void neuralNetwork::read_layer_norm(int ln_id)
{
    //int layer_norm_w_c[30], layer_norm_w_e[30],layer_norm_b_c[30], layer_norm_b_e[30];
    //int layer_norm_w_q_start[30],layer_norm_b_q_start[30];
    const int padded_width = channel_in;
    const int real_width = attn_dim;
    int orgsize=val[0].size() ;
    val[0].resize(orgsize+2*padded_width);
    total_in_size +=2*padded_width;
    const std::string path = indexed_parameter_path("ln_", ln_id);
    std::ifstream param(path, std::ios::binary);
    if (!param) throw std::runtime_error("Cannot open LayerNorm parameter file " + path);
    int32_t width, wc, we, bc, be;
    checked_read(param, &width, sizeof(width), path);
    checked_read(param, &wc, sizeof(wc), path);
    checked_read(param, &we, sizeof(we), path);
    checked_read(param, &bc, sizeof(bc), path);
    checked_read(param, &be, sizeof(be), path);
    if (width != real_width)
        throw std::runtime_error("LayerNorm width mismatch in " + path);
    std::vector<int32_t> weights(width), biases(width);
    checked_read(param, weights.data(), width * sizeof(int32_t), path);
    checked_read(param, biases.data(), width * sizeof(int32_t), path);
    layer_norm_w_c[ln_id]=wc;
    layer_norm_w_e[ln_id]=we;
    layer_norm_b_c[ln_id]=bc;
    layer_norm_b_e[ln_id]=be;
    layer_norm_w_q_start[ln_id]=orgsize;
    layer_norm_b_q_start[ln_id]=orgsize+padded_width;

    for(int i=0;i<padded_width;i++)
    {
        const int32_t value = i < real_width ? weights[i] : 0;
        val[0][i+layer_norm_w_q_start[ln_id]]=value;
        track_val0_bits("ln_w", value);
    }
    for(int i=0;i<padded_width;i++)
    {
        const int32_t value = i < real_width ? biases[i] : 0;
        val[0][i+layer_norm_b_q_start[ln_id]]=value;
        track_val0_bits("ln_b", value);
    }
    // recordInput0Segment("layer_norm_" + std::to_string(ln_id) + "_weight",
    //                     static_cast<size_t>(channel_out),
    //                     static_cast<size_t>(channel_out),
    //                     static_cast<size_t>(layer_norm_w_q_start[ln_id]));
    // recordInput0Segment("layer_norm_" + std::to_string(ln_id) + "_bias",
    //                     static_cast<size_t>(channel_out),
    //                     static_cast<size_t>(channel_out),
    //                     static_cast<size_t>(layer_norm_b_q_start[ln_id]));

}   
std::ostream& operator<<(std::ostream& os, __int128_t value) {
    if (value < 0) {
        os << '-';
        value = -value;
    }
    if (value == 0) {
        os << '0';
        return os;
    }

    std::string digits;
    while (value > 0) {
        digits.push_back(char('0' + (value % 10)));
        value /= 10;
    }
    std::reverse(digits.begin(), digits.end());
    os << digits;
    return os;
}

// 保存中间值总和（sum）、方差（B）、sigma、归一化结果（py)、
// 证明中间值正确性的delta1、delta2
void neuralNetwork::ln_checker_layer1(layer &circuit, i64 &layer_id, int ln_id, int ey,int cy,int real_cn_in,bool* sparsity_map)
{
    
    int cw= layer_norm_w_c[ln_id]; //scale w  //TODO we need to fix all layer_norm value's read
    int ew= layer_norm_w_e[ln_id];
    int cb= layer_norm_b_c[ln_id];  //scale b
    int eb= layer_norm_b_e[ln_id];
    double sw=pow(2,ew)*cw,sb=pow(2,eb)*cb;
    double sy=pow(2,ey)*cy;
    int qw_off= layer_norm_w_q_start[ln_id];  // place of w vector
    int qb_off= layer_norm_b_q_start[ln_id];  // place of b vector
    
    int c1,e1,c2,e2;
    pair<int,int> S1,S2;
    S1=search(sw*sqrt(real_cn_in)/sy); // TODO: s1 is wrong, channel_out should be something else
    S2=search(sb/sy);
    e1=S1.first;
    c1=S1.second;
    e2=S2.first;
    c2=S2.second;
    layer_norm_c1[ln_id]=c1;
    layer_norm_e1[ln_id]=e1;
    layer_norm_c2[ln_id]=c2;
    layer_norm_e2[ln_id]=e2;
    int m=multi_max::max(1,-e1,-e2);
    i64 block_len = len* channel_in;
    int output_size=block_len+3*len;
    initLayer(circuit, output_size, layerType::LAYER_NORM_1);  //TODO: the output dim of the layer
    circuit.need_phase2=true;
    circuit.zero_start_id=block_len+2*len;
    int orgsize=val[0].size() ;
    // 重新构建输入层，添加中间计算的数据
    val[0].resize(orgsize+10+block_len*2+4*len);// y(768*len), sum,B, sigma, delta1, delta2(768*len)
    for(int i=orgsize;i<val[0].size();i++)
        val[0][i]=0;
    ln_aux_start=orgsize; 
    val[0][orgsize]=1; 
    total_relu_in_size += 10+ block_len*2+4*len;
    ll qx[1024]={0},a[1024]={0};
    ll qw[1024],qb[1024];
    for (i64 co = 0; co < channel_in; ++co) 
    {
        qw[co]=convert(val[0][qw_off+co]);  // 在layer_norm里设置为 1
        qb[co]=convert(val[0][qb_off+co]);  // 在layer_norm里设置为 1
    }
    __int128 mn=1;
    for (i64 i = 0; i < len; i++)
    {
        ll sum=0,B=1,sigma;
        if(!sparsity_map[i*channel_in])
        {
            continue;
        }
        // 每一个token，求一个sum  sum=\sum(xi)
        for (i64 co = 0; co < channel_in; ++co) 
        {
            i64 g = matIdx(i, co, channel_in);
            qx[co]=convert(val[0][q_offset+g]);
            sum+=qx[co];
        }
        // a[co]=xi*n-sum
        // B=1+\sum(a[co]^2)
        for (i64 co = 0; co < channel_in; ++co) 
        {
            if(!sparsity_map[i*channel_in+co])
                continue;
            a[co]=qx[co]*real_cn_in-sum;  //TODO here has to change   
            B+=a[co]*a[co];
        }
        assert(B!=0);

        sigma=round(sqrt(static_cast<long double>(B)));
        assert(sigma!=0);
        assert(sigma*sigma+sigma+1-B<(1ll<<32) && sigma*sigma+sigma+1-B>0);
        assert(B-sigma*sigma+sigma<(1ll<<32) && B-sigma*sigma+sigma>0);
        // 为了证明sigma的正确性
        Fr delta1= Fr(sigma*sigma+sigma+1-B)*Fr(B-sigma*sigma+sigma);
        // 计算y=round(w*a/sigma+b) 
        for (i64 co = 0; co < channel_in; ++co) 
        {
            i64 g = matIdx(i, co, channel_in);
            if(!sparsity_map[g])
            {
                continue;
            }
            ll qy = round(pow(2,e1)*c1*qw[co]*a[co]/sigma+pow(2,e2)*c2*qb[co]); // Qbit？
            
            int y_off=orgsize+10+g;
            int d2_off=orgsize+10+block_len+len*4+matIdx(i, co, real_cn_in);
            val[0][y_off]=qy; // 保存归一化的输出值qy
            track_val0_bits("ln_y", qy);
            ll term1,term2;
            term1=(2*qy+1)*(1ll<<(m-1))*sigma+1-(1ll<<(e1+m))*c1*qw[co]*a[co]-(1ll<<(e2+m))*c2*qb[co]*sigma;
            term2=(1ll<<(e1+m))*c1*qw[co]*a[co]+(1ll<<(e2+m))*c2*qb[co]*sigma-(2*qy-1)*(1ll<<(m-1))*sigma+1;
            assert(term1>0&&term2>0);
            // stats::update_max("ln_term1", term1);
            // stats::update_max("ln_term2", term2);
            // stats::update_max("ln_y", qy);
            val[0][d2_off]=Fr(term1)*Fr(term2); // 保存约束条件delta2
            track_val0_bits("ln_delta2", (__int128)term1 * (__int128)term2);

            positive_check+=1;  //add one d2
        }
        // 保存中间计算的结果 sum、B、sigma、delta1
        int sum_off=orgsize+10+block_len+i;
        val[0][sum_off]=sum;
        track_val0_bits("ln_sum", sum);
        int b_off=orgsize+10+block_len+len+i;
        val[0][b_off]=B;  
        track_val0_bits("ln_B", B);
        int sig_off=orgsize+10+block_len+len*2+i;
        val[0][sig_off]=sigma;
        track_val0_bits("ln_sigma", sigma);
        int d1_off=orgsize+10+block_len+len*3+i;
        val[0][d1_off]=delta1;
        track_val0_bits("ln_delta1", convert(delta1));
        positive_check+=1;  //add one d1
    }
    // recordInput0Segment("ln_checker_layer1_aux",
    //                     static_cast<size_t>(10 + 2 * len * real_cn_in + 4 * len),
    //                     static_cast<size_t>(10 + block_len * 2 + 4 * len),
    //                     static_cast<size_t>(orgsize));
    Fr SUM=0;
    // 构建计算中间值的电路
    for (i64 i = 0; i < len; i++)
    {
        if(!sparsity_map[i*channel_in])
        {
            continue;
        }
        int sum_off=orgsize+10+block_len+i;
        int sig_off=orgsize+10+block_len+len*2+i;
        int b_off=orgsize+10+block_len+len+i;
        for (i64 co = 0; co < channel_in; ++co) 
        {
            int g = matIdx(i, co, channel_in);
            if(!sparsity_map[g])
            {
                continue;
            }
            circuit.uni_gates.emplace_back(g, q_offset+g, 0, real_cn_in); // verify a with x and sum 
            circuit.uni_gates.emplace_back(g, sum_off, 0, -1);
            if(i==2)
            {
                SUM+=val[0][q_offset+g];
            }
            circuit.uni_gates.emplace_back(block_len+len*2+i, q_offset+g, 0, 1); //+xi
        }
        circuit.bin_gates.emplace_back(block_len+i, sig_off,sig_off, 1,0);//sigma^2
        circuit.uni_gates.emplace_back(block_len+i, sig_off, 0, 1);  //sigma
        circuit.uni_gates.emplace_back(block_len+i, orgsize, 0, 1); //+1
        circuit.uni_gates.emplace_back(block_len+i, b_off, 0, -1); //-B


        circuit.bin_gates.emplace_back(block_len+len+i, sig_off,sig_off, -1,0);//-sigma^2
        circuit.uni_gates.emplace_back(block_len+len+i, sig_off, 0, 1);  //sigma
        circuit.uni_gates.emplace_back(block_len+len+i, b_off, 0, 1); //+B
        circuit.uni_gates.emplace_back(block_len+len*2+i, sum_off, 0, -1);  //-SUM
    }
    // 根据电路进行计算
    calcNormalLayer(circuit, layer_id);
    for (i64 i = 0; i < len; i++)
    {
        assert(val[layer_id][block_len+len*2+i].isZero());
    }

    layer_id++;
}


// 构建检查delta2、delta1、B计算正确性的电路
void neuralNetwork::ln_checker_layer2(layer &circuit, i64 &layer_id, int ln_id, int ey,int cy,bool* sparsity_map)
{
    int cw= layer_norm_w_c[ln_id]; //scale w  //TODO we need to fix all layer_norm value's read
    int ew= layer_norm_w_e[ln_id];
    int cb= layer_norm_b_c[ln_id];  //scale b
    int eb= layer_norm_b_e[ln_id];
    int qw_off= layer_norm_w_q_start[ln_id];  // place of w vector
    int qb_off= layer_norm_b_q_start[ln_id];  // place of b vector
    int c1,e1,c2,e2;
    c1=layer_norm_c1[ln_id];
    e1=layer_norm_e1[ln_id];
    c2=layer_norm_c2[ln_id];
    e2=layer_norm_e2[ln_id];
    int m=multi_max::max(-e1,-e2,1);
    i64 block_len = len* channel_in;
    int output_size=2*block_len+2*len;
    initLayer(circuit, output_size, layerType::LAYER_NORM_2);  //TODO: the output dim of the layer
    circuit.need_phase2=true;
    circuit.zero_start_id=2*block_len;
    int orgsize=ln_aux_start;
    //ll qx[1024],a[1024];
    //ll qw[1024],qb[1024];
    for (i64 i = 0; i < len; i++)
    {
        int sum_off=orgsize+10+block_len+i;
        int sig_off=orgsize+10+block_len+len*2+i;
        int b_off=orgsize+10+block_len+len+i;

        // (2*qy+1)*(1ll<<(m-1))*sigma+1  -(1ll<<(e1+m))*c1*qw[co]*a[co]  -(1ll<<(e2+m))*c2*qb[co]*sigma;

        ll SUM=0,cir=0;
        for (i64 co = 0; co < channel_in; ++co) 
        {
            int g = matIdx(i, co, channel_in);
            int y_off=orgsize+10+g;
            if(!sparsity_map[g])
            {
                continue;
            }
            // 构建term1
            circuit.bin_gates.emplace_back(g, g, qw_off+co, -(1ll<<(e1+m))*c1 ,2 ); // -qw[co]*a[co]
            circuit.bin_gates.emplace_back(g, sig_off, qb_off+co, -(1ll<<(e2+m))*c2 ,0); //-(1ll<<(e2+m))*c2*qb[co]*sigma;
            circuit.bin_gates.emplace_back(g, sig_off,y_off ,1<<m ,0 );
            circuit.uni_gates.emplace_back(g, orgsize,0 ,1 );
            circuit.uni_gates.emplace_back(g, sig_off,0 ,(1<<(m-1)) );
            // 构建term2
            circuit.bin_gates.emplace_back(g+block_len, g, qw_off+co, (1ll<<(e1+m))*c1 ,2 ); // qw[co]*a[co]
            circuit.bin_gates.emplace_back(g+block_len, sig_off, qb_off+co, (1ll<<(e2+m))*c2 ,0); //(1ll<<(e2+m))*c2*qb[co]*sigma;
            circuit.bin_gates.emplace_back(g+block_len, sig_off,y_off ,-(1<<m) ,0 );
            circuit.uni_gates.emplace_back(g+block_len, orgsize,0 ,1 );
            circuit.uni_gates.emplace_back(g+block_len, sig_off,0 ,(1<<(m-1)) );

            circuit.bin_gates.emplace_back(2*block_len+len+i, g, g ,1,1 );
            cir=convert(val[layer_id-1][g]);
            SUM+=cir*cir;
        }
        if(!sparsity_map[i*channel_in])
        {
            continue;
        }
        ll P=convert(val[0][b_off]);
        // 构建delta1的电路
        circuit.bin_gates.emplace_back(2*block_len+i, block_len+i,block_len+len+i, 1,1);
        int d1_off=orgsize+10+block_len+len*3+i;
        circuit.uni_gates.emplace_back(2*block_len+i, d1_off, 0, -1); 
        // 构建B的电路
        circuit.uni_gates.emplace_back(2*block_len+len+i, orgsize, 0, 1); 
        circuit.uni_gates.emplace_back(2*block_len+len+i, b_off, 0, -1);  //b=sigma^2+1
        /*circuit.bin_gates.emplace_back(block_len+i, sig_off,sig_off, 1,0);//sigma^2
        circuit.uni_gates.emplace_back(block_len+i, sig_off, 0, 1);  //sigma
        circuit.uni_gates.emplace_back(block_len+i, orgsize, 0, 1); //+1
        circuit.uni_gates.emplace_back(block_len+i, b_off, 0, -1); //-B
        circuit.bin_gates.emplace_back(block_len+len+i, sig_off,sig_off, -1,0);//-sigma^2
        circuit.uni_gates.emplace_back(block_len+len+i, sig_off, 0, 1);  //sigma
        circuit.uni_gates.emplace_back(block_len+len+i, b_off, 0, 1); //+B

        circuit.uni_gates.emplace_back(block_len+len*2+i, sum_off, 0, -1);  //-SUM
        */
    }
    // 根据电路进行计算
    calcNormalLayer(circuit, layer_id);
    for (i64 i = 0; i < block_len; i++)
    {
        // 判断term1 term2的正负性来约束delta2的正确性
        assert(!val[layer_id][i].isNegative());  //only need to check the sparse items of these
        assert(!val[layer_id][block_len+i].isNegative());  
        if(i<block_len)
        {
            if(sparsity_map[i]==false)
                assert(val[layer_id][i].isZero());
            if(sparsity_map[i]==false)
                assert(val[layer_id][i+block_len].isZero());
        }
    }
    for (i64 i = 0; i < len; i++)
    {
        int b_off=orgsize+10+block_len+len+i;
        // 判断 B 计算的正确性
        assert(val[layer_id][2*block_len+len+i].isZero());
        // 判断delta1的正负性来约束sigma的正确性
        assert(val[layer_id][2*block_len+i].isZero());
    }

    layer_id++;
}


// 验证 delta2 = term1*term2 
void neuralNetwork::ln_checker_layer3(layer &circuit, i64 &layer_id, int ln_id, int ey,int cy,int real_cn_in,bool* sparsity_map)
{
    i64 block_len = len* channel_in;
    int output_size=len*real_cn_in;
    initLayer(circuit, output_size, layerType::LAYER_NORM_3);  //TODO: the output dim of the layer
    circuit.need_phase2=true;
    circuit.zero_start_id=0;
    int orgsize=ln_aux_start;  // use previous layer's org size
    
    for (i64 i = 0; i < len; i++)
    {
        for (i64 co = 0; co < real_cn_in; ++co) 
        {
            int g=matIdx(i,co,channel_in);
            int p=matIdx(i, co, real_cn_in);
            int d2_off=orgsize+10+block_len+len*4+p;
            int c1=orgsize;
            // 构建电路
            circuit.uni_gates.emplace_back(p, d2_off, 0, -1);  
            circuit.bin_gates.emplace_back(p, g, g+block_len,1,1);
        }
        
    }
    // 根据电路进行计算
    calcNormalLayer(circuit, layer_id);
    for(int g=0;g<output_size;g++)
    {   // 判断计算结果，从而确定delta2 = term1*term2
        assert(val[layer_id][g].isZero());   // assert will fail without UNDEF NDEBUG on cmake
    }
    layer_id++;
    q_offset=ln_aux_start+10;  // get the rounded result for next computation
} 



void neuralNetwork::gelu_checker_layer1(layer &circuit, i64 &layer_id, int real_cn_out, int ea,int ca,int eb,int cb,int ec,int cc,int ed,int cd,int ex,int cx,int ey,int cy)
{
    int m1=multi_max::max(0,-ec,-eb-ex,-ea-2*ex,ex-ey);
    int m2=multi_max::max(0,-ed,-ex);
    ll C1 = cx*(1ll<<(m1));
    ll C2 = (1ll<<(ea +2*ex+m1))*cx*ca*cx*cx;
    ll C3 = (1ll<<(eb+ex+m1))*cx*cb*cx;
    ll    C4 = cx*cc * (1ll<<(ec +m1));
    ll    C5 = (1ll<<(ey-ex+m1))*cy;
    
    ll    C6 = cd * (1ll<<(ed +m2));
    ll    C7 = cx * (1ll<<(ex +m2));

    i64 block_len = len* channel_out;
    int output_size=6*block_len;
    initLayer(circuit, output_size, layerType::GELU_1);  //TODO: the output dim of the layer
    circuit.need_phase2=true;
    circuit.zero_start_id=block_len*2;
    int orgsize=val[0].size() ;
    val[0].resize(orgsize +10 + block_len*3 + len*real_cn_out*3 );// Const; y; abs; t; d1; d2; d3
    for(int i=orgsize;i<val[0].size();i++)
        val[0][i]=0;
    gelu_aux_start=orgsize; 
    positive_check+=len*real_cn_out*3; //add positive check
    total_relu_in_size += 10+ len*real_cn_out*3 + block_len*3;
    for (i64 g = 0; g < block_len; ++g) 
    {
        if(g%channel_out>=real_cn_out)
            continue;
        ll qx=convert(val[0][g+q_offset]);
        
        int y_off=orgsize+10;
        int abs_off=orgsize+10+block_len;
        int t_off=orgsize+10+block_len*2;
        int d1_off=orgsize+10+block_len*3;  
        int d2_off=orgsize+10+block_len*3+ len*real_cn_out;
        int d3_off=orgsize+10+block_len*3+ len*real_cn_out*2;
        
        ll abs,t;
        // 求qx的绝对值
        if(qx<0)
        {
            val[0][abs_off+g]=abs=-qx;
        }
        else
        {
            val[0][abs_off+g]=abs=qx;
        }
        track_val0_bits("gelu_abs", abs);
        // 计算t值
        if(C6>=C7*abs)
        {
            val[0][t_off+g]=t=1;
        }
        else
        {
            val[0][t_off+g]=t=0;
        }
        track_val0_bits("gelu_t", t);

        int gp=g/channel_out* real_cn_out+g%channel_out;
        // delta1证明绝对值正确性
        val[0][d1_off+gp]=abs+1;
        track_val0_bits("gelu_delta1", abs + 1);
        // delta2证明t值正确性
        val[0][d2_off+gp]=t+(1-2*t)*(C7*abs-C6);
        track_val0_bits("gelu_delta2", t + (1 - 2 * t) * (C7 * abs - C6));

        assert(!val[0][d1_off+gp].isNegative());
        assert(!val[0][d2_off+gp].isNegative());
        // 关键计算GELU的式子
        double inner=(double)ca*cx*cx*qx*qx*pow(2,ea+2*ex)-cb*cx*abs*pow(2,eb+ex)+cc*pow(2,ec);
        double middle=(double)qx+abs-abs*t*inner;
        double final=(double)cx*pow(2,ex-1-ey)*middle/cy;
        ll y=round(final);
        val[0][y_off+g]=(ll)y;
        track_val0_bits("gelu_y", y);

        // delta3证明y=round（）的正确性，delta3=term1*term2
        ll term1=(2*y + 1)*C5 + 1 - C1 * qx - C1 * abs + C2 *t*abs*abs*abs-C3 *t* abs*abs+C4 *t* abs;
        ll term2= C1 * qx + C1 * abs - C2 *t*abs*abs*abs+C3 *t* abs*abs-C4 *t* abs-(2*y-1)*C5+1;
        assert(term1>0);
        assert(term2>0);
        // stats::update_max("gelu_term1", term1);
        // stats::update_max("gelu_term2", term2);
        // stats::update_max("gelu_y", y);
        // stats::update_max("gelu_abs", abs);
        __int128 gelu_delta3 = (__int128)term1 * (__int128)term2;
        val[0][d3_off+gp]= Fr(term1)*Fr(term2);
        track_val0_bits("gelu_delta3", gelu_delta3);
        recordGeluDelta3Scalar(gelu_delta3);

    }
    val[0][orgsize]=1; 
    // recordInput0Segment("gelu_checker_layer1_aux",
    //                     static_cast<size_t>(10 + 6 * len * real_cn_out),
    //                     static_cast<size_t>(10 + block_len * 3 + len * real_cn_out * 3),
    //                     static_cast<size_t>(orgsize));
    for (i64 g = 0; g < block_len; ++g) 
    {
        if(g%channel_out>=real_cn_out)
            continue;
        int gp=g/channel_out* real_cn_out+g%channel_out;
        int abs_off=orgsize+10+block_len+g;
        
        int t_off=orgsize+10+block_len*2+g;

        int d1_off=orgsize+10+block_len*3+gp;  
        int d2_off=orgsize+10+block_len*3+ len*real_cn_out+gp;

        int c1=orgsize;

        int q_off=g+q_offset;

        int abs_mult_t_off=g;
        int q_square_off=g+block_len;
        int dt1_off=g+block_len*2;
        int dt2_off=g+block_len*3;
        int t2_off=g+block_len*4;
        int abs_check_off=g+block_len*5;

        circuit.bin_gates.emplace_back(abs_mult_t_off, abs_off, t_off, 1, 0);  // sc, layer
        circuit.bin_gates.emplace_back(q_square_off, q_off, q_off, 1, 0);  // sc, layer
        // 验证d1计算正确性 abs+1-d1=0
        circuit.uni_gates.emplace_back(dt1_off,abs_off,0,1);  //g,u,lu,sc
        circuit.uni_gates.emplace_back(dt1_off,c1,0,1);  //g,u,lu,sc
        circuit.uni_gates.emplace_back(dt1_off,d1_off,0,-1);  //g,u,lu,sc
        // 验证dt2计算正确性
        circuit.uni_gates.emplace_back(dt2_off,c1,0,-C6); 
        circuit.uni_gates.emplace_back(dt2_off,t_off,0,2ll*C6+1); 
        circuit.uni_gates.emplace_back(dt2_off,abs_off,0,C7); 
        circuit.bin_gates.emplace_back(dt2_off, abs_off, t_off, -2ll*C7, 0);
        circuit.uni_gates.emplace_back(dt2_off,d2_off,0,-1);  //g,u,lu,sc
        // t^2-t=0:即t=0或1
        circuit.bin_gates.emplace_back(t2_off, t_off, t_off, 1, 0);  // sc, layer
        circuit.uni_gates.emplace_back(t2_off, t_off, 0,-1);  //g,u,lu,sc
        // 验证 qx^2-abs^2=0
        circuit.bin_gates.emplace_back(abs_check_off, q_off, q_off, 1, 0);  // sc, layer
        circuit.bin_gates.emplace_back(abs_check_off, abs_off, abs_off, -1, 0);  // sc, layer
    }
    calcNormalLayer(circuit, layer_id);
    

    for(int g=0;g<block_len;g++)
    {
        //TODO round for python, work wierd
        assert(!val[layer_id][g].isNegative());   // assert will fail without UNDEF NDEBUG on cmake
        assert(!val[layer_id][g+block_len].isNegative());
        if(g%channel_out>=real_cn_out)
        {
            assert(val[layer_id][g].isZero());   
            assert(val[layer_id][g+block_len].isZero());
        }
        assert(val[layer_id][g+block_len*2].isZero());
        assert(val[layer_id][g+block_len*3].isZero());
        assert(val[layer_id][g+block_len*4].isZero());
        assert(val[layer_id][g+block_len*5].isZero());
    }
    layer_id++;
} 

void neuralNetwork::gelu_checker_layer2(layer &circuit, i64 &layer_id, int real_cn_out,int ea,int ca,int eb,int cb,int ec,int cc,int ed,int cd,int ex,int cx,int ey,int cy)
{
    int m1=multi_max::max(0,-ec,-eb-ex,-ea-2*ex,ex-ey);
    int m2=multi_max::max(0,-ed,-ex);
    ll C1 = cx*(1ll<<(m1));
    ll C2 = (1ll<<(ea +2*ex+m1))*cx*ca*cx*cx;
    ll C3 = (1ll<<(eb+ex+m1))*cx*cb*cx;
    ll    C4 = cx*cc * (1ll<<(ec +m1));
    ll    C5 = (1ll<<(ey-ex+m1))*cy;
    
    ll    C6 = cd * (1ll<<(ed +m2));
    ll    C7 = cx * (1ll<<(ex +m2));
    i64 block_len = len* channel_out;
    int output_size=2*block_len;
    initLayer(circuit, output_size, layerType::GELU_2);  //TODO: the output dim of the layer
    circuit.need_phase2=true;
    int orgsize=gelu_aux_start;  // use previous layer's org size

    for (i64 g = 0; g < block_len; ++g) 
    {
        if(g%channel_out>=real_cn_out)
            continue;
        int y_off=orgsize+10+g;
        int abs_off=orgsize+10+block_len+g;
        
        int t_off=orgsize+10+block_len*2+g;
        int c1=orgsize;

        int q_off=g+q_offset;

        int term1_off=g;
        int term2_off=g+block_len;

        int abs_mult_t_off=g;
        int q_square_off=g+block_len;
        // term1构造
        circuit.uni_gates.emplace_back(term1_off, c1, 0, C5+1);  // C5+1
        circuit.uni_gates.emplace_back(term1_off, y_off, 0, C5*2);  // 2*C5*y
        circuit.uni_gates.emplace_back(term1_off, abs_off, 0, -C1);  
        circuit.uni_gates.emplace_back(term1_off, q_off, 0, -C1);  
        circuit.uni_gates.emplace_back(term1_off, abs_mult_t_off, layer_id-1, C4);  
        circuit.bin_gates.emplace_back(term1_off, abs_mult_t_off,q_square_off,C2,1);
        circuit.bin_gates.emplace_back(term1_off, q_square_off, t_off,-C3,2);

        // term2构造
        circuit.uni_gates.emplace_back(term2_off, c1, 0, C5+1);  // C5+1
        circuit.uni_gates.emplace_back(term2_off, y_off, 0, -C5*2);  // 2*C5*y
        circuit.uni_gates.emplace_back(term2_off, abs_off, 0, C1);  
        circuit.uni_gates.emplace_back(term2_off, q_off, 0, C1);  
        circuit.uni_gates.emplace_back(term2_off, abs_mult_t_off, layer_id-1, -C4);  
        circuit.bin_gates.emplace_back(term2_off, abs_mult_t_off,q_square_off,-C2,1);
        circuit.bin_gates.emplace_back(term2_off, q_square_off, t_off,C3,2);
    }
    calcNormalLayer(circuit, layer_id);
    for(int g=0;g<block_len;g++)
    {
        
        if(g%channel_out>=real_cn_out)
        {
            assert(val[layer_id][g].isZero());   
            assert(val[layer_id][g+block_len].isZero());
        }
        assert(!val[layer_id][g].isNegative());   // assert will fail without UNDEF NDEBUG on cmake
        assert(!val[layer_id][g+block_len].isNegative());
    }
    layer_id++;
} 


void neuralNetwork::gelu_checker_layer3(layer &circuit, i64 &layer_id,int real_cn_out, int ea,int ca,int eb,int cb,int ec,int cc,int ed,int cd,int ex,int cx,int ey,int cy)
{

    i64 block_len = len* channel_out;
    int output_size=len* real_cn_out;
    initLayer(circuit, output_size, layerType::GELU_3);  //TODO: the output dim of the layer
    circuit.need_phase2=true;
    circuit.zero_start_id=0;
    int orgsize=gelu_aux_start;  // use previous layer's org size

    for (i64 g = 0; g < block_len; ++g) 
    {
        if(g%channel_out>=real_cn_out)
            continue;
        int gp=g/channel_out* real_cn_out+g%channel_out;

        int d3_off=orgsize+10+block_len*3+2*real_cn_out*len+gp;
        int c1=orgsize;

        circuit.uni_gates.emplace_back(gp, d3_off, 0, -1);  
        circuit.bin_gates.emplace_back(gp, g, g+block_len,1,1);
    }
    calcNormalLayer(circuit, layer_id);
    for(int g=0;g<output_size;g++)
        assert(val[layer_id][g].isZero());   
    layer_id++;
    q_offset=gelu_aux_start+10;  
} 


// we place the computation of after fcon layer here
void neuralNetwork::roundLayer(layer &circuit, i64 &layer_id, float scale,
                               bool* sparsity_map, int bias_offset)
{
    i64 block_len = len* channel_out;
    int c,m;
    pair<int,int> pm=search(scale);
    m=pm.first;
    c=pm.second;
    // printf("round scale: %f, c: %d, m: %d\n",scale,c,m);  0.00349 1 -10
    float virtual_scale=c*pow(2,m);
    i64 size = block_len; 
    initLayer(circuit, size, layerType::RELU);  //TODO: the output dim of the layer
    circuit.need_phase2=true;
    circuit.zero_start_id=0;
    int orgsize=val[0].size() ;
    val[0].resize(orgsize + 20 + block_len*2);// Const; Q; delta
    
    q_offset=orgsize + 20 ;  //TODO set the input offset of the next matrix
    total_relu_in_size += 20+ block_len*2; //TODO: need to update here, for all aux vars added
    val[0][orgsize]=1; 
    int M=max(-m,0);
    for(i64 g = 0; g < block_len; ++g) 
    {
        int qq=g+orgsize+20;
        double fm;
        double fz;
        // p是上一层FC的输出
        ll p=convert(val[layer_id-1][g]);
        ll bias = bias_offset >= 0
            ? convert(val[0][bias_offset + (g % channel_out)]) : 0;
        ll affine_p = p + bias;
        // q是round量化后的结果
        ll q=round(affine_p*c*pow(2,m));
        val[0][qq]=q;  // compute non-linear round
        track_val0_bits("round_q", q);
        // stats::update_max("round_q", q);
        int s=qq+block_len;
        // 保存delta的值来证明round的正确性
        long long term1 = affine_p*c*(1ll<<(m+M+1)) + (1ll<<M) - q*(1ll<<(M+1));
        long long term2 = q*(1ll<<(M+1)) + (1ll<<M) - c*(1ll<<(m+M+1))*affine_p;
        // stats::update_max("round_term1", term1);
        // stats::update_max("round_term2", term2);
        __int128 round_delta = (__int128)term1 * (__int128)term2;
        val[0][s]=Fr(term1) * Fr(term2);
        track_val0_bits("round_delta", round_delta);
        recordRoundDeltaScalar(round_delta);
        assert(!val[0][s].isNegative());
    }
    // recordInput0Segment("roundLayer_aux",
    //                     static_cast<size_t>(20 + block_len * 2),
    //                     static_cast<size_t>(20 + block_len * 2),
    //                     static_cast<size_t>(orgsize));
    for (i64 g = 0; g < block_len; ++g) 
    {
        int p=g;
        int q=g+orgsize+20;
        int c1=orgsize;
        int s=q+block_len;
        if(sparsity_map)
        {
            if(!sparsity_map[g])
                continue;
        }
        // 构造验证delta正确性的电路
        circuit.bin_gates.emplace_back(g, p, p, -(1ll<<(2*m+2*M+2))*c*c , 1);
        circuit.bin_gates.emplace_back(g, p, q, (1ll<<(m+2*M+3))*c ,2);  //
        circuit.bin_gates.emplace_back(g, q, q, -(1<<(2*M+2)) ,0);
        if (bias_offset >= 0) {
            const int b = bias_offset + (g % channel_out);
            // Substitute (p+bias) for p in the fused affine-round relation.
            circuit.bin_gates.emplace_back(g, p, b,
                -(1ll<<(2*m+2*M+3))*c*c, 2);
            circuit.bin_gates.emplace_back(g, b, b,
                -(1ll<<(2*m+2*M+2))*c*c, 0);
            circuit.bin_gates.emplace_back(g, b, q,
                (1ll<<(m+2*M+3))*c, 0);
        }
        circuit.uni_gates.emplace_back(g, c1, 0, (1ll<<(2*M)) );  // this public input is one
        circuit.uni_gates.emplace_back(g, s, 0, -1);
    }   
    // 计算
    calcNormalLayer(circuit, layer_id);
    for(int i=0;i<block_len;i++)
    {
        int p=i;
        int q=i+orgsize+20;
        // 判断delta的正负性来约束round的正确性
        assert(val[layer_id][i].isZero());   
    }
    layer_id++;
}

void neuralNetwork::residualLayer(layer &circuit, i64 &layer_id,
                                  int residual_offset, int element_count,
                                  const string &name)
{
    if (residual_offset < 0 || q_offset < 0)
        throw std::runtime_error("Invalid offsets for " + name);

    const int input_offset = q_offset;
    const int output_offset = static_cast<int>(val[0].size());
    val[0].resize(output_offset + element_count);
    total_relu_in_size += element_count;

    initLayer(circuit, element_count, layerType::RELU);
    circuit.need_phase2 = false;
    circuit.zero_start_id = 0;

    for (int g = 0; g < element_count; ++g) {
        val[0][output_offset + g] =
            val[0][input_offset + g] + val[0][residual_offset + g];
        // The circuit output is identically zero iff the public quantized
        // residual equation y = x + residual is satisfied.
        circuit.uni_gates.emplace_back(g, output_offset + g, 0, 1);
        circuit.uni_gates.emplace_back(g, input_offset + g, 0, -1);
        circuit.uni_gates.emplace_back(g, residual_offset + g, 0, -1);
    }

    calcNormalLayer(circuit, layer_id);
    for (int g = 0; g < element_count; ++g)
        assert(val[layer_id][g].isZero());
    ++layer_id;
    q_offset = output_offset;
}


void neuralNetwork::multi_head_matrix_QK(layer &circuit, i64 &layer_id)
{
    const int HEAD=12;
    const int HSIZE=64;
    int output_size=HEAD*len*(len+1)/2;
    initLayer(circuit, output_size, layerType::MHA_QK);
    circuit.need_phase2 = true;
    for(int head=0;head<HEAD;head++)
    {
        int T=0;
        for(int i=0;i<len;i++)
        for(int j=0;j<=i;j++)
        {
            int targ_gate=head*len*(len+1)/2+T;
            for(int k=0;k<HSIZE;k++){
                // (i, head*64+k)
                // (j, HEAD*HSIZE+head*64+k)
                int col_i=head*64+k;
                int col_j=HEAD*HSIZE+head*64+k;
                int idi=i*channel_out+col_i;
                int idj=j*channel_out+col_j;
                int gate_i=q_offset+idi;
                int gate_j=q_offset+idj;
                // record actual input values for statistics (safe: use convert)
                ll vi = convert(val[0][gate_i]);
                ll vj = convert(val[0][gate_j]);
                // stats::update_max("input_val_i", vi);
                // stats::update_max("input_val_j", vj);
                circuit.bin_gates.emplace_back(targ_gate, gate_i,gate_j, 1,0 );
            }
            ++T;
        }
    }
    calcNormalLayer(circuit, layer_id);
    layer_id++;
}

// 计算e的i次方，并且输出结果已经量化
void neuralNetwork::compute_e_table()
{
    double St=pow(2,-9),Se=pow(2,-20);
    table.resize(EXP_TABLE_SIZE);
    for(int i=0;i<EXP_TABLE_SIZE;i++)
    {
        int t=round(exp(-St*i)/Se);
        table[i]=max(t,1);  //TODO: avoid sum_Ei=0, occasionally happens
    }
}
// 计算中间值
// 证明pmax>pj，构建证明比较结果正确性的电路
// 证明sumE的正确性
// 证明term1>0,term2>0(查找表索引量化正确性：delta2=term1*term2)
void neuralNetwork::softmax_layer_1(layer &circuit, i64 &layer_id,float SQ,float SK,float Sv, float Sy)
{
    const int HEAD=12;
    const int HSIZE=64;
    ll max_abs_score = 0;
    ll max_pj = 0;
    ll max_tj = 0;
    int orgsize=val[0].size();
    val[0].resize(orgsize+10+HEAD*2*len+4*HEAD*len*(len+1)/2+HEAD*len*HSIZE+len*channel_in);//sumE,pmax（每个位置i的最大分数）,delta1,delta2,t,E,delta3,Y
    for(int i=orgsize;i<val[0].size();i++)
        val[0][i].clear();
    val[0][orgsize]=1;
    softmax_aux_start=orgsize;
    positive_check+=2*HEAD*len*(len+1)/2+HEAD*len*HSIZE; //add positive check for delta1,delta2,delta3、正性检查
    exp_check+=HEAD*len*(len+1)/2;  //exp check for (t,E) pair、指数检查
    total_relu_in_size += 10+2*HEAD*len+4*HEAD*len*(len+1)/2+HEAD*len*HSIZE+len*channel_in;
    int output_size=3*HEAD*len*(len+1)/2+HEAD*len+HEAD*len*HSIZE;
    initLayer(circuit, output_size, layerType::SOFTMAX_1);
    circuit.need_phase2 = true;
    circuit.zero_start_id=2*HEAD*len*(len+1)/2+HEAD*len*HSIZE;
    int e1,c1;
    const float St=pow(2,-9);
    const float Se=pow(2,-16);
    pair<int,int> pm=search(SQ*SK/St);
    e1=pm.first;
    c1=pm.second;
    int eprime=max(-e1-1,0);
    for(int head=0;head<HEAD;head++)
    {
        for(int i=1;i<=len;i++)
        {
            int sum_E_offset=orgsize+10+head*len+i-1;
            int pmax_offset=orgsize+10+HEAD*len+head*len+i-1;
            val[0][sum_E_offset]=0;
            //for(int j=0;j<i;j++)
            //{
            //    int offset=head*len*(len+1)/2+T;
            //    val[0][sum_E_offset]+=val[layer_id-1][offset];
            //}
            ll mx=0;
            // 找到每个head,每个位置i的最大分数pmax
            for(int j=0;j<i;j++)
            {
                int offset=head*len*(len+1)/2+i*(i-1)/2+j;
                ll S=convert(val[layer_id-1][offset]);
                max_abs_score = max(max_abs_score, S >= 0 ? S : -S);
                mx=max(mx,S);
            }
            val[0][pmax_offset]=Fr(mx);
            track_val0_bits("softmax_pmax", mx);
            for(int j=0;j<i;j++) // i is length, j is id
            {
                int offset=head*len*(len+1)/2+i*(i-1)/2+j; //global offset
                int dt1_off=orgsize+10+HEAD*2*len+offset;
                int dt2_off=orgsize+10+HEAD*2*len+HEAD*len*(len+1)/2+offset;
                //int dt3_off=orgsize+10+HEAD*2*len+2*HEAD*len*(len+1)/2+offset;
                int t_off=orgsize+10+HEAD*2*len+2*HEAD*len*(len+1)/2+offset;
                int E_off=orgsize+10+HEAD*2*len+3*HEAD*len*(len+1)/2+offset;
                val[0][dt1_off]=val[0][pmax_offset]-val[layer_id-1][offset]; //delta1=pmax-pj，用来判断pmax > pj
                ll pj_=convert(val[0][dt1_off]);
                max_pj = max(max_pj, pj_);
                track_val0_bits("softmax_delta1", pj_);
                // 把pj_=pmax-pj放缩成整数索引tj
                ll tj=round(c1*pow(2,e1+eprime+1)*pj_/pow(2,eprime+1));
                max_tj = max(max_tj, tj);
                val[0][t_off]=tj;
                track_val0_bits("softmax_t", tj);
                if (tj < 0) {
                    std::cout << "softmax overflow: "
                            << "head=" << head
                            << ", i=" << i
                            << ", j=" << j
                            << ", pj_=" << pj_
                            << ", tj=" << tj
                            << ", coeff=" << ((__int128)c1) << "*2^" << e1
                            << "\n";
                    assert(false);
                }

                // 通过查表得到e的pj_次方的近似值
                int exp_value = tj < EXP_TABLE_SIZE ? table[tj] : 1;
                val[0][E_off]=exp_value;
                track_val0_bits("softmax_E", exp_value);
                // delta2 判断 tj=round（pmax-pj）
                ll sm_t1 = c1 * (1 << (e1 + eprime + 1)) * pj_ + (1 << eprime) - tj * (1 << (eprime + 1));
                ll sm_t2 = -c1 * (1 << (e1 + eprime + 1)) * pj_ + (1 << eprime) + tj * (1 << (eprime + 1));
                val[0][dt2_off]=Fr(sm_t1) * Fr(sm_t2);
                track_val0_bits("softmax_delta2", (__int128)sm_t1 * (__int128)sm_t2);

                // sumE
                val[0][sum_E_offset]+=exp_value;
                ++T;
            }
            track_val0_bits("softmax_sumE", convert(val[0][sum_E_offset]));
        }
    }
    // std::cout << "softmax stats: coeff=" << ((__int128)c1) << "*2^" << e1
    //           << ", max|score|=" << max_abs_score
    //           << ", max_pj=" << max_pj
    //           << ", max_tj=" << max_tj << "\n";
    for(int head=0;head<HEAD;head++)
    {
        for(int i=0;i<len;i++)
        {
            for(int j=0;j<HSIZE;j++)
            {
                int out_ij=head*len*HSIZE+i*HSIZE+j;
                //  计算E * V
                for(int k=0;k<=i;k++)
                {
                    int Vkj_offset=q_offset+channel_out*k+2*HEAD*HSIZE+head*HSIZE+j; //Vkj, offset   
                    int E_off=orgsize+10+HEAD*2*len+3*HEAD*len*(len+1)/2+head*len*(len+1)/2+(i*(i+1))/2+k;
                    circuit.bin_gates.emplace_back(out_ij, Vkj_offset,E_off,1,0);
                }
            }
        }
        
        for(int i=1;i<=len;i++)
        {
            int pmax_offset=orgsize+10+HEAD*len+head*len+i-1;
            int sum_E_offset=orgsize+10+head*len+i-1;
            for(int j=0;j<i;j++)
            {
                int offset=head*len*(len+1)/2+(i*(i-1))/2+j; //global offset
                int dt1_off=orgsize+10+HEAD*2*len+offset;
                // 构建delta1的电路，检查pmax的正确性
                int check_seg1_offset=2*HEAD*len*(len+1)/2+HEAD*len*HSIZE+offset;
                circuit.uni_gates.emplace_back(check_seg1_offset, dt1_off,0,-1); //-(pm-pi)
                circuit.uni_gates.emplace_back(check_seg1_offset, offset,layer_id-1,-1); //-pi
                circuit.uni_gates.emplace_back(check_seg1_offset, pmax_offset,0,1); //+pmax
                int term1_offset=HEAD*len*HSIZE+offset;
                int term2_offset=HEAD*len*(len+1)/2+HEAD*len*HSIZE+offset;
                int t_off=orgsize+10+HEAD*2*len+2*HEAD*len*(len+1)/2+offset;
                // 构建term1、term2的电路（delta2=term1*term2），检查t结果的正确性
                circuit.uni_gates.emplace_back(term1_offset, dt1_off,0,c1*(1<<(e1+1+eprime))); //c1*2^(e1+1+e')
                circuit.uni_gates.emplace_back(term1_offset, orgsize,0,1<<eprime); //+2^e'
                circuit.uni_gates.emplace_back(term1_offset, t_off,0,-(1<<(eprime+1))); //-2^(e'+1)*ti

                circuit.uni_gates.emplace_back(term2_offset, dt1_off,0,-c1*(1<<(e1+1+eprime))); //c1*2^(e1+1+e')
                circuit.uni_gates.emplace_back(term2_offset, orgsize,0,1<<eprime); //+2^e'
                circuit.uni_gates.emplace_back(term2_offset, t_off,0,(1<<(eprime+1))); //-2^(e'+1)*ti
            }
            // 检查sumE的正确性
            int sum_e_check=3*HEAD*len*(len+1)/2+HEAD*len*HSIZE+head*len+i-1;
            circuit.uni_gates.emplace_back(sum_e_check, sum_E_offset,0,-1); //-2^(e'+1)*ti
            for(int j=0;j<i;j++) // i is length, j is id
            {
                int offset=head*len*(len+1)/2+(i*(i-1))/2+j; //global offset
                int E_off=orgsize+10+HEAD*2*len+3*HEAD*len*(len+1)/2+offset;
                circuit.uni_gates.emplace_back(sum_e_check, E_off,0,1);
            }
        }
    }
    
    calcNormalLayer(circuit, layer_id);
    for(int i=circuit.zero_start_id;i<val[layer_id].size();i++)
    {   // 检查sumE是否为0
        assert(val[layer_id][i].isZero());
    }
    for(int i=HEAD*len*HSIZE;i<circuit.zero_start_id;i++)
    {
        // 判断term1和term2是否为正数、判断delta1是否为正数
        assert(!val[layer_id][i].isNegative());
    }
    layer_id++;
}
// 证明d1>0,d2>0(证明softmax输出结果正确性的表达式：delta3=d1*d2)
// 证明delta2-term1*term2=0，检查 查找表索引量化的正确性
void neuralNetwork::softmax_layer_2(layer &circuit, i64 &layer_id,float SQ,float SK,float Sv, float Sy)
{
    const int HEAD=12;
    const int HSIZE=64;
    int orgsize=softmax_aux_start;
    
    int output_size=2*HEAD*len*HSIZE+HEAD*len*(len+1)/2; //delta3_term1, delta3_term2, delta2_check 
    initLayer(circuit, output_size, layerType::SOFTMAX_2);
    circuit.need_phase2 = true;
    circuit.zero_start_id=2*HEAD*len*HSIZE;
    int e1,c1;
    pair<int,int> pm=search(Sv/Sy);
    e1=pm.first;
    c1=pm.second;
    int eprime=max(-e1,1);
    ll f=0,f2=0;
    for(int head=0;head<HEAD;head++)
    {
        for(int i=0;i<len;i++)
        {
            int sum_E_offset=orgsize+10+head*len+i;
            ll sumE=convert(val[0][sum_E_offset]);
            assert(sumE!=0);
            for(int j=0;j<HSIZE;j++)
            {
                int out_ij=head*len*HSIZE+i*HSIZE+j; //on layer_id-1
                int term1_off=out_ij;
                int term2_off=out_ij+HEAD*len*HSIZE;
                int s_ij=orgsize+10+HEAD*2*len+4*HEAD*len*(len+1)/2+HEAD*len*HSIZE+i*channel_in+head*HSIZE+j;
                int d3_off=orgsize+10+HEAD*2*len+4*HEAD*len*(len+1)/2+head*len*HSIZE+i*HSIZE+j;
                ll Qij=convert(val[layer_id-1][out_ij]);
                // 计算 S=Qij/sumE
                ll S=(ll)round(Qij*c1*pow(2,e1)/sumE);
                val[0][s_ij]=S;
                // track_val0_bits("softmax_S", S);
                // stats::update_max("softmax_S", S);
                // stats::update_max("softmax_sumE", sumE);
                // stats::update_max("softmax_Qij", Qij);
                // delta3=d1*d2
                ll d1=(sumE*(1<<(eprime-1))+Qij*c1*(1<<(eprime+e1))-S*sumE*(1<<eprime));
                ll d2=(sumE*(1<<(eprime-1))-Qij*c1*(1<<(eprime+e1))+S*sumE*(1<<eprime));
                // stats::update_max("softmax_d1", d1);
                // stats::update_max("softmax_d2", d2);
                val[0][d3_off]=Fr(d1)*Fr(d2);
                track_val0_bits("softmax_delta3", (__int128)d1 * (__int128)d2);
                f2=min(f2,S);
                // 构建d1的电路
                circuit.uni_gates.emplace_back(term1_off, out_ij,layer_id-1,c1*(1ll<<(e1+eprime)));
                circuit.uni_gates.emplace_back(term1_off, sum_E_offset,0,1ll<<(eprime-1));
                circuit.bin_gates.emplace_back(term1_off, s_ij, sum_E_offset,-(1<<eprime),0);
                // 构建d2的电路
                circuit.uni_gates.emplace_back(term2_off, out_ij,layer_id-1,-c1*(1ll<<(e1+eprime)));
                circuit.uni_gates.emplace_back(term2_off, sum_E_offset,0,1ll<<(eprime-1));
                circuit.bin_gates.emplace_back(term2_off, s_ij, sum_E_offset,(1<<eprime),0);
            }
        }
    }
    for(int head=0;head<HEAD;head++)
    {        
        for(int i=1;i<=len;i++)
        {
            for(int j=0;j<i;j++)
            {
                // 计算term1*term2-delta2=0的电路
                int offset=head*len*(len+1)/2+i*(i-1)/2+j; //global offset
                int dt2_off=orgsize+10+HEAD*2*len+HEAD*len*(len+1)/2+offset;
                int term1_offset=HEAD*len*HSIZE+offset;
                int term2_offset=HEAD*len*(len+1)/2+HEAD*len*HSIZE+offset;
                int now_off=2*HEAD*len*HSIZE+offset;
                circuit.bin_gates.emplace_back(now_off, term1_offset, term2_offset,1,1);
                circuit.uni_gates.emplace_back(now_off,dt2_off,0,-1);
            }
        }
    }
    
    
    calcNormalLayer(circuit, layer_id);
    for(int i=circuit.zero_start_id;i<val[layer_id].size();i++)
    {
        // 判断L1里delta2是否成立
        assert(val[layer_id][i].isZero());
    }
    for(int i=0;i<circuit.zero_start_id;i++)
    {
        // 判断d1、d2是否为正数（delta3=d1*d2）
        assert(!val[layer_id][i].isNegative());
    }
    {
        const size_t tri = static_cast<size_t>(HEAD) * static_cast<size_t>(len) *
                           static_cast<size_t>(len + 1) / 2;
        const size_t head_len = static_cast<size_t>(HEAD) * static_cast<size_t>(len);
        const size_t head_hidden = head_len * static_cast<size_t>(HSIZE);
        // recordInput0Segment("softmax_aux",
        //                     static_cast<size_t>(10) + 2 * head_len + 4 * tri + 2 * head_hidden,
        //                     static_cast<size_t>(10) + 2 * head_len + 4 * tri +
        //                         head_hidden + static_cast<size_t>(len) * static_cast<size_t>(channel_in),
        //                     static_cast<size_t>(orgsize));
    }
    layer_id++;
    
}
// 证明softmax最后输出量化的正确性，delta3-d1*d2=0
void neuralNetwork::softmax_layer_3(layer &circuit, i64 &layer_id,float SQ,float SK,float Sv, float Sy)
{
    const int HEAD=12;
    const int HSIZE=64;
    int orgsize=softmax_aux_start;
    
    int output_size=HEAD*len*HSIZE; //delta3_check
    initLayer(circuit, output_size, layerType::SOFTMAX_3);
    circuit.need_phase2 = true;
    circuit.zero_start_id=0;
    for(int head=0;head<HEAD;head++)
    {
        for(int i=0;i<len;i++)
        {
            int sum_E_offset=orgsize+10+head*len+i;
            ll sumE=convert(val[0][sum_E_offset]);
            assert(sumE!=0);
            for(int j=0;j<HSIZE;j++)
            {
                int now=head*len*HSIZE+i*HSIZE+j; //on layer_id-1
                int term1_off=now;
                int term2_off=now+HEAD*len*HSIZE;
                int d3_off=orgsize+10+HEAD*2*len+4*HEAD*len*(len+1)/2+head*len*HSIZE+i*HSIZE+j;
                //构建delta3-d1*d2=0的电路
                circuit.bin_gates.emplace_back(now,term1_off, term2_off, 1,1);
                circuit.uni_gates.emplace_back(now,d3_off ,0,-1);
            }
        }
    }

    calcNormalLayer(circuit, layer_id);
    for(int i=circuit.zero_start_id;i<val[layer_id].size();i++)
    {
        assert(val[layer_id][i].isZero());
    }
    layer_id++;
    q_offset=orgsize+10+HEAD*2*len+4*HEAD*len*(len+1)/2+HEAD*len*HSIZE; 
}

void neuralNetwork::fullyConnLayer(layer &circuit, i64 &layer_id, i64 first_fc_id,  int x_offset, int x_layer) 
{
    i64 size = channel_out*len;
    initLayer(circuit, size, layerType::FCONN);
    circuit.need_phase2 = true;
    val[layer_id].resize(circuit.size);
    for (i64 i = 0; i < len; i++)
    {
        for (i64 co = 0; co < channel_out; ++co) 
        {
            i64 g = matIdx(i, co, channel_out);
            val[layer_id][g]=0;
            //circuit.uni_gates.emplace_back(g, first_bias_id + co, 0, 1);  // our protocol doesn't support adding bias for simplicity
            for (i64 ci = 0; ci < channel_in; ++ci) 
            {
                i64 u = x_offset+matIdx(i, ci, channel_in);
                i64 v = first_fc_id + matIdx(co, ci, channel_in);  // the matrix is distributed as (i,ci)*(co,ci)
                val[layer_id][g]+=val[x_layer][u]*val[0][v];
            }
        }
    }
    layer_id++;
}


void neuralNetwork::refreshFCParam(const fconKernel &fc) {
    channel_in = fc.channel_in;
    channel_out = fc.channel_out;
}

i64 neuralNetwork::getFFTLen() const {
    return 1L << getFFTBitLen();
}

i8 neuralNetwork::getFFTBitLen() const {
    return 0;
}


void neuralNetwork::calcSizeAfterPool(const poolKernel &p) {
}

// 将输入的浮点数据转换为有限域元素，为后续的零知识证明计算做准备。
void neuralNetwork::calcInputLayer(layer &circuit) 
{
    val[0].resize(circuit.size);

    assert(val[0].size() == total_in_size);
    auto val_0 = val[0].begin();

    double num, mx = -10000, mn = 10000;
    vector<double> input_dat;
    int hidden=768;
    double random_input_abs = 350.0;
    if (const char *env = std::getenv("ZKGPT_RANDOM_INPUT_ABS")) {
        double configured = std::atof(env);
        if (configured > 0) {
            random_input_abs = configured;
        }
    }
    for (i64 i=0;i<len;i++)
    {
        for(i64 j=0;j<hidden;j++)
        {
            //in >> num; 
            num = static_cast<double>(rand()) / static_cast<double>(RAND_MAX) * (2 * random_input_abs) - random_input_abs;
            // printf("input %d: %f\t", i*hidden+j, num);
            input_dat.push_back(num);
            mx = max(mx, num);
            mn = min(mn, num);
        }
    }
    pair<int,int> pm=search(0.01);  
    input_e=pm.first;
    input_c=pm.second;
    
    double sc=input_c*pow(2,input_e);
    // input_c=10,input_e=-10
    // printf("input scale: %f, c: %d, e: %d\n", sc, input_c, input_e);
    int k=0;
    for (i64 i=0;i<len;i++)
    {
        for(i64 j=0;j<hidden;j++)
        {
            ll s=input_dat[k++]/sc;
            val[0][i*1024+j] = F(s); // Q bit？
            track_val0_bits("input_x", s);
            if(i==j)
                std::cout << "input " << (k - 1) << ": " << input_dat[k - 1]
                          << ", scaled: " << s << "/" << convert(val[0][i * 1024 + j]) << "\n";
        }
        for(i64 j=hidden;j<1024;j++)
            val[0][i*1024+j] =0;
    }

    val_0=val[0].begin()+len*1024;
    for (; val_0 < val[0].begin() + circuit.size; ++val_0) 
        val_0 -> clear();
    // recordInput0Segment("input_values",
    //                     static_cast<size_t>(len) * static_cast<size_t>(hidden),
    //                     static_cast<size_t>(len) * 1024,
    //                     0);
}



void neuralNetwork::readBias(i64 first_bias_id) {
    auto val_0 = val[0].begin() + first_bias_id;

    double num, mx = -10000, mn = 10000;
    vector<double> input_dat;
    for (i64 co = 0; co < channel_out; ++co) 
    {
        in >> num;
        input_dat.push_back(num);
        mx = max(mx, num);
        mn = min(mn, num);
    }

    for (double i : input_dat)  
        *val_0++ = F((i64) (i * exp2(w_bit + x_bit)));

}

void neuralNetwork::readFconBias(i64 first_bias_id, int real_count, int id)
{
    const std::string path = indexed_parameter_path("fc_", id, "_bias.bin");
    std::ifstream param(path, std::ios::binary);
    if (!param) throw std::runtime_error("Cannot open GPT-2 FC bias file " + path);
    int32_t count = 0;
    checked_read(param, &count, sizeof(count), path);
    if (count != real_count)
        throw std::runtime_error("FC bias width mismatch in " + path);
    std::vector<int32_t> bias(count);
    checked_read(param, bias.data(), count * sizeof(int32_t), path);

    for (int i = 0; i < channel_out; ++i) {
        const int32_t value = i < real_count ? bias[i] : 0;
        val[0][first_bias_id + i] = value;
        track_val0_bits("fc_bias", value);
    }
}
namespace {
std::string get_fc_weight_path(int id) {
    std::ostringstream path;
    path << parameter_dir() << "/fc_" << id << ".bin";
    return path.str();
}

bool allow_random_fc_weights() {
    const char *flag = std::getenv("ZKGPT_ALLOW_RANDOM_FC_WEIGHTS");
    return flag && std::string(flag) == "1";
}
} // namespace

// 读取量化后的 GPT-2 全连接权重。文件格式：
// int32 rows(out_dim), int32 cols(in_dim), int32 data[rows * cols]，row-major。
void neuralNetwork::readFconWeight(i64 first_fc_id,int real_r,int real_c,int id) 
{
    auto val_0 = val[0].begin() + first_fc_id;
    mat_values[id]=new int[channel_out*channel_in];
    std::fill(mat_values[id], mat_values[id] + channel_out * channel_in, 0);

    std::string weight_path = get_fc_weight_path(id);
    std::ifstream weight_file(weight_path, std::ios::binary);
    std::vector<int32_t> raw_weights;

    if (weight_file) {
        int32_t rows = 0;
        int32_t cols = 0;
        weight_file.read(reinterpret_cast<char*>(&rows), sizeof(rows));
        weight_file.read(reinterpret_cast<char*>(&cols), sizeof(cols));

        if (!weight_file) {
            throw std::runtime_error("Failed to read weight header from " + weight_path);
        }
        if (rows != real_c || cols != real_r) {
            std::ostringstream err;
            err << "GPT-2 FC weight shape mismatch in " << weight_path
                << ": file has [" << rows << ", " << cols << "]"
                << ", expected [" << real_c << ", " << real_r << "]";
            throw std::runtime_error(err.str());
        }

        raw_weights.resize(static_cast<size_t>(rows) * static_cast<size_t>(cols));
        weight_file.read(
            reinterpret_cast<char*>(raw_weights.data()),
            static_cast<std::streamsize>(raw_weights.size() * sizeof(int32_t))
        );
        if (!weight_file) {
            throw std::runtime_error("Failed to read full weight payload from " + weight_path);
        }
    } else if (!allow_random_fc_weights()) {
        throw std::runtime_error(
            "Cannot open GPT-2 FC weight file " + weight_path +
            ". Run scripts/export_gpt2_fc_weights.py first, set ZKGPT_FC_WEIGHT_DIR, "
            "or set ZKGPT_ALLOW_RANDOM_FC_WEIGHTS=1 for old random-weight debugging."
        );
    }

    for (i64 co = 0; co < channel_out; ++co)
        for (i64 ci = 0; ci < channel_in; ++ci) 
        {
            if(co<real_c && ci<real_r)
            {
                
                    //mat_values[id][co*channel_in+ci] = (rand() % 65) - 32;
                    mat_values[id][co*channel_in+ci] = raw_weights[co*real_r+ci];
                val_0[co*channel_in+ci]=mat_values[id][co*channel_in+ci];
                track_val0_bits("fc_weight", mat_values[id][co*channel_in+ci]);
            }
            else
            {
                mat_values[id][co*channel_in+ci]=0;
                val_0[co*channel_in+ci]=0;
            }
        }
    // recordInput0Segment("fc_" + std::to_string(id) + "_weight",
    //                     static_cast<size_t>(real_r) * static_cast<size_t>(real_c),
    //                     static_cast<size_t>(channel_in) * static_cast<size_t>(channel_out),
    //                     static_cast<size_t>(first_fc_id));
}

void neuralNetwork::prepareDecmpBit(i64 layer_id, i64 idx, i64 dcmp_id, i64 bit_shift) {
    auto data = abs(val[layer_id].at(idx).getInt64());
    val[0].at(dcmp_id) = (data >> bit_shift) & 1;
}

void neuralNetwork::prepareFieldBit(const F &data, i64 dcmp_id, i64 bit_shift) {
    auto tmp = abs(data.getInt64());
    val[0].at(dcmp_id) = (tmp >> bit_shift) & 1;
}

void neuralNetwork::prepareSignBit(i64 layer_id, i64 idx, i64 dcmp_id) {
    val[0].at(dcmp_id) = val[layer_id].at(idx).isNegative() ? F_ONE : F_ZERO;
}

void neuralNetwork::prepareMax(i64 layer_id, i64 idx, i64 max_id) {
    auto data = val[layer_id].at(idx).isNegative() ? F_ZERO : val[layer_id].at(idx);
    if (data > val[0].at(max_id)) val[0].at(max_id) = data;
}

void neuralNetwork::calcNormalLayer(const layer &circuit, i64 layer_id,bool output) 
{
    val[layer_id].resize(circuit.size);
    for (auto &x: val[layer_id]) 
        x.clear();
    for (auto &gate: circuit.uni_gates) 
    {
        val[layer_id].at(gate.g) = val[layer_id].at(gate.g) + val[gate.lu].at(gate.u) * gate.sc;
    }

    for (auto &gate: circuit.bin_gates) 
    {
        u8 bin_lu = gate.getLayerIdU(layer_id), bin_lv = gate.getLayerIdV(layer_id);
        
        val[layer_id].at(gate.g) = val[layer_id].at(gate.g) + val[bin_lu].at(gate.u) * val[bin_lv][gate.v] * gate.sc;
    }
}

void neuralNetwork::checkNormalLayer(const layer &circuit, i64 layer_id,const vector<vector<F> > & val) 
{
    vector<F> valp;

    valp.resize(val[layer_id].size());
    
    for (auto &x: valp) 
        x.clear();
    for (auto &gate: circuit.uni_gates) 
    {
        assert(gate.g>=0 && gate.g<valp.size());
        assert(gate.u>=0 && gate.u<val[gate.lu].size());
        valp.at(gate.g) += val[gate.lu].at(gate.u) * gate.sc;
    }
    for (auto &gate: circuit.bin_gates) 
    {
        u8 bin_lu = gate.getLayerIdU(layer_id), bin_lv = gate.getLayerIdV(layer_id);
        assert(gate.g>=0 && gate.g<valp.size());
        valp.at(gate.g)+=  val[bin_lu].at(gate.u) * val[bin_lv][gate.v] * gate.sc;
    }
    for(int i=0;i<circuit.size;i++)
        assert(valp[i]==val[layer_id][i]);
}

void neuralNetwork::calcDotProdLayer(const layer &circuit, i64 layer_id) {
    val[layer_id].resize(circuit.size);
    for (int i = 0; i < circuit.size; ++i) val[layer_id][i].clear();

    char fft_bit = circuit.fft_bit_length;
    u32 fft_len = 1 << fft_bit;
    u8 l = layer_id - 1;
    for (auto &gate: circuit.bin_gates)
        for (int s = 0; s < fft_len; ++s)
            val[layer_id][gate.g << fft_bit | s] = val[layer_id][gate.g << fft_bit | s] +
                    val[l][gate.u << fft_bit | s] * val[l][gate.v << fft_bit | s];
}


int neuralNetwork::getNextBit(int layer_id) {
    F mx = F_ZERO, mn = F_ZERO;
    for (const auto &x: val[layer_id]) {
        if (!x.isNegative()) mx = max(mx, x);
        else mn = max(mn, -x);
    }
    i64 x = (mx + mn).getInt64();
    double real_scale = x / exp2(x_bit + w_bit);
    int res = (int) log2( ((1 << (Q - 1)) - 1) / real_scale );
    return res;
}

void neuralNetwork::printLayerValues(prover &pr) {
    for (i64 i = 0; i < SIZE; ++i) 
    {
        for (i64 j = 0; j < std::min(200u, pr.C.circuit[i].size); ++j)
            if (!pr.val[i][j].isZero()) cerr << pr.val[i][j] << ' ';
        cerr << endl;
        for (i64 j = pr.C.circuit[i].zero_start_id; j < pr.C.circuit[i].size; ++j)
            if (pr.val[i].at(j) != F_ZERO) 
            {
                exit(EXIT_FAILURE);
            }
    }
}

void neuralNetwork::printInfer(prover &pr) {
    // output the inference result with the size of (pic_parallel x n_class)
    if (out.is_open()) 
    {
        int n_class = full_conn.back().channel_out;
        for (int p = 0; p < pic_parallel; ++p) {
            int k = -1;
            F v;
            for (int c = 0; c < n_class; ++c) {
                auto tmp = val[SIZE - 1].at(matIdx(p, c, n_class));
                if (!tmp.isNegative() && (k == -1 || v < tmp)) {
                    k = c;
                    v = tmp;
                }
            }
            out << k << endl;
        }
    }
    out.close();
}
