#include "prover.hpp"
#include <iostream>
#include <utils.hpp>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdint>
//#include <Python.h>
#include "cnpy.h"
#include <mcl/bn.hpp>

static vector<F> beta_gs, beta_u;
using namespace mcl::bn;
using std::unique_ptr;

linear_poly interpolate(const F &zero_v, const F &one_v) 
{
    return {one_v - zero_v, zero_v};
}

F prover::getCirValue(u32 layer_id, const vector<u32> &ori, u32 u) {
    return !layer_id ? val.at(0).at(ori.at(u)) : val.at(layer_id).at(u);
}

void prover::init() 
{
    proof_size = 0;
    r_u.resize(C.size + 1);
    r_v.resize(C.size + 1);
    const int SIZE=28;
    V_mult[0].resize(1<<SIZE);
    V_mult[1].resize(1<<SIZE);
    mult_array[0].resize(1<<SIZE);
    mult_array[1].resize(1<<SIZE);
    tmp_V_mult[0].resize(1<<SIZE);
    tmp_V_mult[1].resize(1<<SIZE);
    tmp_mult_array[0].resize(1<<SIZE);
    tmp_mult_array[1].resize(1<<SIZE);
    lasso_mult_v.resize(1<<SIZE);
    for(int i=0;i<(1<<SIZE);i++)
        lasso_mult_v[i]=0;
}

/**
 * This is to initialize all process.
 *
 * @param the random point to be evaluated at the output layer
 */
void prover::sumcheckInitAll(const vector<F>::const_iterator &r_0_from_v) 
{
    if (C.size == 0 || C.circuit.size() != C.size)
        throw std::runtime_error("sumcheckInitAll: invalid circuit size");
    sumcheck_id = C.size;
    const i8 last_bl = C.circuit.at(sumcheck_id - 1).bit_length;
    r_u.at(sumcheck_id).resize(last_bl);
    prove_timer.start();
    for (int i = 0; i < last_bl; ++i) 
        r_u.at(sumcheck_id).at(i) = r_0_from_v[i];
    prove_timer.stop();
}

/**
 * This is to initialize before the process of a single layer.
 *
 * @param the random combination coefficiants for multiple reduction points
 */
void prover::sumcheckInit(const F &alpha_0, const F &beta_0) 
{
    prove_timer.start();
    if (sumcheck_id == 0 || sumcheck_id > C.size)
        throw std::out_of_range("sumcheckInit: invalid sumcheck layer");
    alpha = alpha_0;
    beta = beta_0;
    r_0 = r_u.at(sumcheck_id).begin();
    r_1 = r_v.at(sumcheck_id).begin();
    --sumcheck_id;
    prove_timer.stop();
}
static ThreadSafeQueue<int> workerq,endq;

void sc_phase1_uni_worker(vector<uniGate> &beg, std::vector<linear_poly> (&mult_array)[2],vector<F>& beta_g, vector<F>& beta_u,int*&L,int*&R) //F*& uni_value, layer &cur_layer,
{
    int idx;
    while (true)
    {
        bool ret=workerq.TryPop(idx);
        if(ret==false)
            return;
        int l=L[idx],r=R[idx];

        for (size_t i = l; i <r ; ++i) 
        {
            auto &gate = beg[i];
            bool idx = gate.lu != 0;
            mult_array[idx][gate.u] = mult_array[idx][gate.u] + beta_g[gate.g] * gate.sc;
        }
        endq.Push(idx);
    }
}

void sc_phase1_bin_worker(layer& cur, vector<binGate> &beg, std::vector<linear_poly> (&mult_array)[2],F& V_u0,F&V_u1,vector<vector<F> >& val,vector<F>& beta_g, vector<F>& beta_u,int*&L,int*&R,u32 sumcheck_id) //F*& uni_value, layer &cur_layer,
{
    int idx;
    while (true)
    {
        bool ret=workerq.TryPop(idx);
        if(ret==false)
            return;
        int l=L[idx],r=R[idx];

        for (size_t i = l; i <r ; ++i) 
        {
            auto &gate = beg[i];
            bool idx = gate.getLayerIdU(sumcheck_id) != 0;
            auto val_lv =  !gate.getLayerIdV(sumcheck_id) ? val[0][cur.ori_id_v[gate.v]] : val[gate.getLayerIdV(sumcheck_id)][gate.v];
            mult_array[idx][gate.u] = mult_array[idx][gate.u] + val_lv * beta_g[gate.g] * gate.sc;  // Ahg for phase 1
        }
        endq.Push(idx);
    }
}

void prover::sumcheckInitPhase1(const F &relu_rou_0) 
{
    //fprintf(stderr, "sumcheck level %d, phase1 init start\n", sumcheck_id);
    auto &cur = C.circuit[sumcheck_id];
    total[0] = ~cur.bit_length_u[0] ? 1ULL << cur.bit_length_u[0] : 0;
    total_size[0] = cur.size_u[0];
    total[1] = ~cur.bit_length_u[1] ? 1ULL << cur.bit_length_u[1] : 0;
    total_size[1] = cur.size_u[1];

    r_u[sumcheck_id].resize(cur.max_bl_u);
    timer useless_t;
    useless_t.start();
    beta_g.resize(1ULL << cur.bit_length);
    relu_rou = relu_rou_0;
    add_term.clear();
    
    for (int b = 0; b < 2; ++b)
        for (u32 u = 0; u < total[b]; ++u)
            mult_array[b][u].clear();
    
    
        for (int b = 0; b < 2; ++b) 
        {
            auto dep = !b ? 0 : sumcheck_id - 1;
            for (u32 u = 0; u < total[b]; ++u) 
            {
                if (u >= cur.size_u[b])
                    V_mult[b][u].clear();
                else 
                {
                    V_mult[b][u] = getCirValue(dep, cur.ori_id_u, u);
                }
            }
        }
    useless_t.stop();
    throw_time[sumcheck_id].push_back(useless_t.elapse_sec());
    prove_timer.start();
    
    initBetaTable(beta_g, cur.bit_length, r_0, r_1, alpha, beta);
    if (cur.zero_start_id < cur.size) {
        const u32 padded_size = 1ULL << cur.bit_length;
        for (u32 g = cur.zero_start_id; g < padded_size; ++g)
            beta_g.at(g) *= relu_rou;
    }
    
    if(cur.uni_interval.size()>=2)
    {
        const int thd=32;
        int *L=new int [cur.uni_interval.size()],*R=new int [cur.uni_interval.size()];
        for (u64 j = 0; j <cur.uni_interval.size(); ++j) 
        {
                workerq.Push(j);
                L[j]=cur.uni_interval[j].first;
                R[j]=cur.uni_interval[j].second;
        }
        vector<thread> workers;
        for(int i=0;i<thd;i++)
            workers.emplace_back(sc_phase1_uni_worker, std::ref(cur.uni_gates),
                std::ref(mult_array), std::ref(beta_g), std::ref(beta_u),
                std::ref(L), std::ref(R));
        while(!workerq.Empty())
            this_thread::sleep_for (std::chrono::microseconds(1));
        while(endq.Size()!=cur.uni_interval.size())
            this_thread::sleep_for (std::chrono::microseconds(1));
        for (auto &worker : workers) worker.join();
        endq.Clear();
        delete[] L;
        delete[] R;
    }
    else for (auto &gate: cur.uni_gates) 
        {
            bool idx = gate.lu != 0;
            mult_array[idx][gate.u] = mult_array[idx][gate.u] + beta_g[gate.g] * gate.sc;
        }
    if(cur.bin_interval.size()>=2)
    {
        const int thd=32;
        int *L=new int [cur.bin_interval.size()],*R=new int [cur.bin_interval.size()];
        for (u64 j = 0; j <cur.bin_interval.size(); ++j) 
        {
                workerq.Push(j);
                L[j]=cur.bin_interval[j].first;
                R[j]=cur.bin_interval[j].second;
        }
        vector<thread> workers;
        for(int i=0;i<thd;i++)
            workers.emplace_back(sc_phase1_bin_worker, std::ref(cur),
                std::ref(cur.bin_gates), std::ref(mult_array), std::ref(V_u0),
                std::ref(V_u1), std::ref(val), std::ref(beta_g),
                std::ref(beta_u), std::ref(L), std::ref(R), sumcheck_id);
        while(!workerq.Empty())
            this_thread::sleep_for (std::chrono::microseconds(1));
        while(endq.Size()!=cur.bin_interval.size())
            this_thread::sleep_for (std::chrono::microseconds(1));
        for (auto &worker : workers) worker.join();
        endq.Clear();
        delete[] L;
        delete[] R;
    }
    else  for (auto &gate: cur.bin_gates) 
        {
            bool idx = gate.getLayerIdU(sumcheck_id) != 0;
            auto val_lv = getCirValue(gate.getLayerIdV(sumcheck_id), cur.ori_id_v, gate.v);
            mult_array[idx][gate.u] = mult_array[idx][gate.u] + val_lv * beta_g[gate.g] * gate.sc;  // Ahg for phase 1
        }
    round = 0;
    prove_timer.stop();
    //fprintf(stderr, "sumcheck level %d, phase1 init finished\n", sumcheck_id);
}


void sc_phase2_uni_worker( vector<uniGate> &beg, F& sum_value,F& V_u0,F&V_u1,vector<F>& beta_g, vector<F>& beta_u,int*&L,int*&R) //F*& uni_value, layer &cur_layer,
{
    int idx;
    while (true)
    {
        bool ret=workerq.TryPop(idx);
        if(ret==false)
            return;
        int l=L[idx],r=R[idx];
        F ss;
        ss.clear();
        for (size_t i = l; i <r ; ++i) 
        {
            auto &gate = beg[i];
            auto V_u = !gate.lu ? V_u0 : V_u1;                  //V_u0 is claim 0, V_u1 is claim 1
            ss +=beta_g[gate.g] * beta_u[gate.u] * V_u * gate.sc;
        }
        sum_value+=ss;
        endq.Push(idx);
    }
}

void sc_phase2_bin_worker( vector<binGate> &beg, std::vector<linear_poly> (&mult_array)[2],F& V_u0,F&V_u1,vector<F>& beta_g, vector<F>& beta_u,int*&L,int*&R,u32 sumcheck_id) //F*& uni_value, layer &cur_layer,
{
    int idx;
    while (true)
    {
        bool ret=workerq.TryPop(idx);
        if(ret==false)
            return;
        int l=L[idx],r=R[idx];

        for (size_t i = l; i <r ; ++i) 
        {
            auto &gate = beg[i];
            bool idx = gate.getLayerIdV(sumcheck_id);
            auto V_u = !gate.getLayerIdU(sumcheck_id) ? V_u0 : V_u1;
            mult_array[idx][gate.v] =mult_array[idx][gate.v]+beta_g[gate.g] * beta_u[gate.u] * V_u * gate.sc;
        }
        endq.Push(idx);
    }
}
void prover::sumcheckInitPhase2() 
{
    // fprintf(stderr, "sumcheck level %d, phase2 init start\n", sumcheck_id);
    auto &cur = C.circuit[sumcheck_id];
    total[0] = ~cur.bit_length_v[0] ? 1ULL << cur.bit_length_v[0] : 0;
    total_size[0] = cur.size_v[0];
    total[1] = ~cur.bit_length_v[1] ? 1ULL << cur.bit_length_v[1] : 0;
    total_size[1] = cur.size_v[1];
    i8 fft_bl = cur.fft_bit_length;
    i8 cnt_bl = cur.max_bl_v;

    timer useless_time;
    useless_time.start();
    r_v[sumcheck_id].resize(cur.max_bl_v);


    beta_u.resize(1ULL << cur.max_bl_u);

    

    add_term.clear();
    for (int b = 0; b < 2; ++b) 
    {
        for (u32 v = 0; v < total[b]; ++v)
            mult_array[b][v].clear();
    }
    useless_time.stop();
    throw_time[sumcheck_id].push_back(useless_time.elapse_sec());
    prove_timer.start();
    initBetaTable(beta_u, cur.max_bl_u, r_u[sumcheck_id].begin(), F_ONE,32); //  beta_u is U in the code
    for (int b = 0; b < 2; ++b) 
    {
        auto dep = !b ? 0 : sumcheck_id - 1;
        for (u32 v = 0; v < total[b]; ++v) 
        {
            V_mult[b][v] = v >= cur.size_v[b] ? F_ZERO : getCirValue(dep, cur.ori_id_v, v);
        }
    }
    
    if(cur.uni_interval.size()>=2)
    {
        const int thd=32;
        F sum[40];
        
        int *L=new int [cur.uni_interval.size()],*R=new int [cur.uni_interval.size()];
        for (u64 j = 0; j <cur.uni_interval.size(); ++j) 
        {
                workerq.Push(j);
                L[j]=cur.uni_interval[j].first;
                R[j]=cur.uni_interval[j].second;
        }
            vector<thread> workers;
            for(int i=0;i<thd;i++)
            {
                sum[i].clear();
                workers.emplace_back(sc_phase2_uni_worker,
                    std::ref(cur.uni_gates), std::ref(sum[i]), std::ref(V_u0),
                    std::ref(V_u1), std::ref(beta_g), std::ref(beta_u),
                    std::ref(L), std::ref(R));
            }
            while(!workerq.Empty())
                this_thread::sleep_for (std::chrono::microseconds(1));
            while(endq.Size()!=cur.uni_interval.size())
                this_thread::sleep_for (std::chrono::microseconds(1));
            for (auto &worker : workers) worker.join();
            endq.Clear();
            for(int i=0;i<thd;i++)
                add_term+=sum[i];
            delete[] L;
            delete[] R;
    }
    else for (auto &gate: cur.uni_gates) 
    {
        auto V_u = !gate.lu ? V_u0 : V_u1;                  //V_u0 is claim 0, V_u1 is claim 1
        add_term = add_term + beta_g[gate.g] * beta_u[gate.u] * V_u * gate.sc;
    }
    if(cur.bin_interval.size()>=2)
    {
        const int thd=32;
        int *L=new int [cur.bin_interval.size()],*R=new int [cur.bin_interval.size()];
        for (u64 j = 0; j <cur.bin_interval.size(); ++j) 
        {
                workerq.Push(j);
                L[j]=cur.bin_interval[j].first;
                R[j]=cur.bin_interval[j].second;
        }
            vector<thread> workers;
            for(int i=0;i<thd;i++)
                workers.emplace_back(sc_phase2_bin_worker,
                    std::ref(cur.bin_gates), std::ref(mult_array),
                    std::ref(V_u0), std::ref(V_u1), std::ref(beta_g),
                    std::ref(beta_u), std::ref(L), std::ref(R), sumcheck_id);
            while(!workerq.Empty())
                this_thread::sleep_for (std::chrono::microseconds(1));
            while(endq.Size()!=cur.bin_interval.size())
                this_thread::sleep_for (std::chrono::microseconds(1));
            for (auto &worker : workers) worker.join();
            endq.Clear();
            delete[] L;
            delete[] R;
    }
    else for (auto &gate: cur.bin_gates) 
    {
        bool idx = gate.getLayerIdV(sumcheck_id);
        auto V_u = !gate.getLayerIdU(sumcheck_id) ? V_u0 : V_u1;
        mult_array[idx][gate.v] = mult_array[idx][gate.v] + beta_g[gate.g] * beta_u[gate.u] * V_u * gate.sc;
    }

    round = 0;
    prove_timer.stop();
}

void prover::sumcheckLassoInit(
    const vector<F> &s_u,
    const vector<F> &s_v,
    const vector<vector<F>> &r_uu,
    const vector<vector<F>> &r_vv)
{
    prove_timer.start();
    sumcheck_id = 0;
    if (C.size == 0 || C.circuit.size() != C.size)
        throw std::runtime_error("sumcheckLassoInit: invalid circuit size");
    if (s_u.size() < C.size - 1 || s_v.size() < C.size - 1)
        throw std::length_error("sumcheckLassoInit: sigma vector is too short");
    if (r_u.size() <= C.size - 1 || r_v.size() <= C.size - 1)
        throw std::length_error("sumcheckLassoInit: prover random-point table is too short");

    const layer &input_layer = C.circuit.at(0);
    if (input_layer.bit_length < 0)
        throw std::runtime_error("sumcheckLassoInit: invalid input bit length");
    total[1] = 1ULL << static_cast<u32>(input_layer.bit_length);
    total_size[1] = input_layer.size;
    r_u.at(0).resize(static_cast<u32>(input_layer.bit_length));

    i8 max_bl = 0;
    for (u32 i = 1; i < C.size; ++i) {
        const layer &cur = C.circuit.at(i);
        max_bl = std::max(max_bl,
                          std::max(cur.bit_length_u[0], cur.bit_length_v[0]));
    }
    beta_g.resize(1ULL << static_cast<u32>(max_bl));

    auto copy_random_point = [](
        const vector<vector<F>> &source, vector<vector<F>> &target,
        u32 layer_id, u32 required, const char *name) {
        if (layer_id >= source.size() || layer_id >= target.size()) {
            std::ostringstream error;
            error << "sumcheckLassoInit: " << name << " layer " << layer_id
                  << " is outside source/target tables (source=" << source.size()
                  << ", target=" << target.size() << ')';
            throw std::out_of_range(error.str());
        }
        const vector<F> &source_point = source.at(layer_id);
        if (source_point.size() < required) {
            std::ostringstream error;
            error << "sumcheckLassoInit: " << name << '[' << layer_id
                  << "] has " << source_point.size() << " coordinates, expected "
                  << required;
            throw std::length_error(error.str());
        }
        target.at(layer_id).assign(source_point.begin(),
                                   source_point.begin() + required);
    };

    for (u32 i = 1; i < C.size; ++i) {
        const layer &cur = C.circuit.at(i);
        const i8 bit_length_u = cur.bit_length_u[0];
        const u32 size_u = cur.size_u[0];
        if (bit_length_u >= 0) {
            const u32 required_u = static_cast<u32>(cur.max_bl_u);
            if (required_u < static_cast<u32>(bit_length_u) ||
                size_u > cur.ori_id_u.size() ||
                size_u > (1ULL << static_cast<u32>(bit_length_u))) {
                std::ostringstream error;
                error << "sumcheckLassoInit: inconsistent U metadata at layer " << i;
                throw std::length_error(error.str());
            }
            copy_random_point(r_uu, r_u, i, required_u, "r_u");
            initBetaTable(beta_g, bit_length_u, r_u.at(i).begin(),
                          s_u.at(i - 1), 32);
            for (u32 hu = 0; hu < size_u; ++hu) {
                const u32 u = cur.ori_id_u.at(hu);
                if (u >= lasso_mult_v.size()) {
                    std::ostringstream error;
                    error << "sumcheckLassoInit: U target " << u
                          << " at layer " << i << " exceeds lasso input size "
                          << lasso_mult_v.size();
                    throw std::out_of_range(error.str());
                }
                lasso_mult_v.at(u) += beta_g.at(hu);
            }
        }

        const i8 bit_length_v = cur.bit_length_v[0];
        const u32 size_v = cur.size_v[0];
        if (bit_length_v >= 0) {
            const u32 required_v = static_cast<u32>(cur.max_bl_v);
            if (required_v < static_cast<u32>(bit_length_v) ||
                size_v > cur.ori_id_v.size() ||
                size_v > (1ULL << static_cast<u32>(bit_length_v))) {
                std::ostringstream error;
                error << "sumcheckLassoInit: inconsistent V metadata at layer " << i;
                throw std::length_error(error.str());
            }
            copy_random_point(r_vv, r_v, i, required_v, "r_v");
            initBetaTable(beta_g, bit_length_v, r_v.at(i).begin(),
                          s_v.at(i - 1), 32);
            for (u32 hv = 0; hv < size_v; ++hv) {
                const u32 v = cur.ori_id_v.at(hv);
                if (v >= lasso_mult_v.size()) {
                    std::ostringstream error;
                    error << "sumcheckLassoInit: V target " << v
                          << " at layer " << i << " exceeds lasso input size "
                          << lasso_mult_v.size();
                    throw std::out_of_range(error.str());
                }
                lasso_mult_v.at(v) += beta_g.at(hv);
            }
        }
    }

    round = 0;
    prove_timer.stop();
}

quadratic_poly prover::sumcheckUpdate1(const F &previous_random) {
    return sumcheckUpdate(previous_random, r_u[sumcheck_id]);
}

quadratic_poly prover::sumcheckUpdate2(const F &previous_random) {
    return sumcheckUpdate(previous_random, r_v[sumcheck_id]);
}

quadratic_poly prover::sumcheckUpdate(const F &previous_random, vector<F> &r_arr) 
{
    prove_timer.start();

    if (round) r_arr.at(round - 1) = previous_random;
    ++round;
    quadratic_poly ret;

    add_term = add_term * (F_ONE - previous_random);
    for (int b = 0; b < 2; ++b)
        ret = ret + sumcheckUpdateEach(previous_random, b);
    ret = ret + quadratic_poly(F_ZERO, -add_term, add_term);

    prove_timer.stop();
    proof_size += F_BYTE_SIZE * 3;
    return ret;
}



void sumcheckUpdate_worker(quadratic_poly &sum,vector<linear_poly> &tmp_v,vector<linear_poly> &tmp_mult,vector<linear_poly> &tmp_v_2,vector<linear_poly> &tmp_mult_2,int*&L,int*&R,Fr previous_random,int total_size)
{
    int idx;
    while (true)
    {
        bool ret=workerq.TryPop(idx);
        if(ret==false)
            return;
        int l=L[idx],r=R[idx];
        for(int i=l;i<r;i++)
        {
            u32 g0 = i << 1, g1 = i << 1 | 1;
            if (g0 >= total_size) 
                break;
            tmp_v_2[i] = interpolate(tmp_v[g0].eval(previous_random), tmp_v[g1].eval(previous_random));
            tmp_mult_2[i] = interpolate(tmp_mult[g0].eval(previous_random), tmp_mult[g1].eval(previous_random));
            sum = sum + tmp_mult_2[i] * tmp_v_2[i];
        }
        endq.Push(idx);
    }
}
quadratic_poly prover::sumcheckUpdateEach(const F &previous_random, bool idx) 
{
    auto &tmp_mult = mult_array[idx];
    auto &tmp_v = V_mult[idx];
    auto &tmp_mult_2 = tmp_mult_array[idx];
    auto &tmp_v_2 = tmp_V_mult[idx];

    if (total[idx] == 1) 
    {
        tmp_v[0] = tmp_v[0].eval(previous_random);
        tmp_mult[0] = tmp_mult[0].eval(previous_random);
        add_term = add_term + tmp_v[0].b * tmp_mult[0].b;
    }

    quadratic_poly ret;
    ret.clear();
    // The shared-queue implementation below is not yet proven
    // equivalent for sparse mixed-source layers. Keep the sound path
    // deterministic; Stage A prioritizes witness integrity over throughput.
    if(total[idx]<(1U<<29))
    {
        for (u32 i = 0; i < (total[idx] >> 1); ++i) 
        {
            u32 g0 = i << 1, g1 = i << 1 | 1;
            if (g0 >= total_size[idx]) 
            {
                tmp_v[i].clear();
                tmp_mult[i].clear();
                continue;
            }
            tmp_v[i] = interpolate(tmp_v[g0].eval(previous_random), tmp_v[g1].eval(previous_random));
            tmp_mult[i] = interpolate(tmp_mult[g0].eval(previous_random), tmp_mult[g1].eval(previous_random));
            ret = ret + tmp_mult[i] * tmp_v[i];
        }
    }
    else
    {
        timer tt_f1,tt_f2;
        tt_f1.start();
        const int k=10;
        int total_work=(total[idx] >> 1);
        int *L=new int [(1<<k)],*R=new int [(1<<k)];
        const int thd=32;
        for (u64 j = 0; j < (1<<k); ++j) 
        {
            workerq.Push(j);
            L[j]=(total_work>>k)*j;
            R[j]=(total_work>>k)*(1+j);
        }
        quadratic_poly qp[thd];
        for (int j=0; j<thd; ++j)
            qp[j].clear();
        vector<thread> workers;
        for(int j=0;j<thd;j++)
            workers.emplace_back(sumcheckUpdate_worker, std::ref(qp[j]),
                std::ref(tmp_v), std::ref(tmp_mult), std::ref(tmp_v_2),
                std::ref(tmp_mult_2), std::ref(L), std::ref(R),
                previous_random, total_size[idx]);
        while(endq.Size()!=(1<<k))
            this_thread::sleep_for(std::chrono::microseconds(1));
        for (auto &worker : workers) worker.join();
        endq.Clear();
        for(int j=0;j<thd;j++)
            ret=ret+qp[j];
        delete[] L;
        delete[] R;
        tt_f1.stop();
        tt_f2.start();
        for(int i=0;i<total_work;i++)
        {
            if((i<<1)<total_size[idx])
            {
                tmp_mult[i]=tmp_mult_2[i];
                tmp_v[i]=tmp_v_2[i];
            }
            else
            {
                tmp_mult[i].clear();
                tmp_v[i].clear();
            }
        }
        tt_f2.stop();
    }
    
    total[idx] >>= 1;
    total_size[idx] = (total_size[idx] + 1) >> 1;

    return ret;
}


/**
 * This is to evaluate a multi-linear extension at a random point.
 *
 * @param the value of the array & random point & the size of the array & the size of the random point
 * @return sum of `values`, or 0.0 if `values` is empty.
 */
F prover::Vres(const vector<F>::const_iterator &r, u32 output_size, u32 r_size, u32 layer_id, u32 start) 
{
    prove_timer.start();

    const vector<F> &layer_values = val.at(layer_id);
    if (start > layer_values.size() ||
        output_size > layer_values.size() - start)
        throw std::out_of_range("Vres: requested range exceeds layer values");
    vector<F> output(output_size);
    for (u32 i = 0; i < output_size; ++i)
        output.at(i) = layer_values.at(i + start);
    u32 whole = 1ULL << r_size;
    for (u32 i = 0; i < r_size; ++i) {
        for (u32 j = 0; j < (whole >> 1); ++j) {
            if (j > 0)
                output[j].clear();
            if ((j << 1) < output_size)
                output[j] = output[j << 1] * (F_ONE - r[i]);
            if ((j << 1 | 1) < output_size)
                output[j] = output[j] + output[j << 1 | 1] * (r[i]);
        }
        whole >>= 1;
    }
    F res = output[0];

    prove_timer.stop();
    proof_size += F_BYTE_SIZE;
    return res;
}

void prover::sumcheckFinalize1(const F &previous_random, F &claim_0, F &claim_1) {
    prove_timer.start();
    r_u[sumcheck_id].at(round - 1) = previous_random;
    V_u0 = claim_0 = total[0] ? V_mult[0][0].eval(previous_random) : (~C.circuit[sumcheck_id].bit_length_u[0]) ? V_mult[0][0].b : F_ZERO;
    V_u1 = claim_1 = total[1] ? V_mult[1][0].eval(previous_random) : (~C.circuit[sumcheck_id].bit_length_u[1]) ? V_mult[1][0].b : F_ZERO;
    prove_timer.stop();

    proof_size += F_BYTE_SIZE * 2;
}

void prover::sumcheckFinalize2(const F &previous_random, F &claim_0, F &claim_1) {
    prove_timer.start();
    r_v[sumcheck_id].at(round - 1) = previous_random;
    claim_0 = total[0] ? V_mult[0][0].eval(previous_random) : (~C.circuit[sumcheck_id].bit_length_v[0]) ? V_mult[0][0].b : F_ZERO;
    claim_1 = total[1] ? V_mult[1][0].eval(previous_random) : (~C.circuit[sumcheck_id].bit_length_v[1]) ? V_mult[1][0].b : F_ZERO;
    prove_timer.stop();

    proof_size += F_BYTE_SIZE * 2;
}

void prover::sumcheck_lasso_Finalize(const F &previous_random, F &claim_1) 
{
    prove_timer.start();
    r_u[sumcheck_id].at(round - 1) = previous_random;
    claim_1 = total[1] ? V_mult[1][0].eval(previous_random) : V_mult[1][0].b;
    prove_timer.stop();
    proof_size += F_BYTE_SIZE;
}

// std::string int128_to_string(__int128 x)
// {
//     if (x == 0) return "0";

//     bool neg = x < 0;
//     if (neg) x = -x;

//     std::string s;
//     while (x > 0) {
//         int digit = x % 10;
//         s.push_back('0' + digit);
//         x /= 10;
//     }

//     if (neg) s.push_back('-');
//     std::reverse(s.begin(), s.end());
//     return s;
// }
/**
 * 写入 vi(ll数组) + G1数组 到文本文件（十进制格式）
 * 文件格式（文本/十进制）：
 * [第1行] vi_len（十进制，单独一行）
 * [第2行] vi 数据（十进制，元素之间用空格分隔，整行）
 * [第3行] g1_count（十进制，单独一行）
 * [第4行开始] 每个G1点占一行，x,y坐标用逗号分隔（十进制大数，BN254有限域元素）
 */
// bool write_ec_data_to_file(const std::string& filename, const ll* vi, size_t vi_len, const mcl::bn::G1* g1_data, size_t g1_count)
// {

//     if (!vi || vi_len == 0 || !g1_data || g1_count == 0) {
//         std::cerr << "错误：输入参数无效（空指针或长度为0）" << std::endl;
//         return false;
//     }

//     std::ofstream file(filename);
//     if (!file.is_open()) {
//         std::cerr << "错误：无法打开文件 " << filename << " 用于写入十进制文本" << std::endl;
//         return false;
//     }

//     uint64_t vi_len_u64 = static_cast<uint64_t>(vi_len);
//     file << vi_len_u64 << std::endl; // 修改2：二进制写入 → 十进制流输出，换行分隔

//     for (size_t i = 0; i < vi_len; ++i) {
//         file << int128_to_string(vi[i]);
//         // 元素之间加空格，最后一个元素后不加空格（可选，不影响解析）
//         if (i != vi_len - 1) {
//             file << " ";
//         }
//     }
//     file << std::endl; 

//     uint64_t g1_count_u64 = static_cast<uint64_t>(g1_count);
//     file << g1_count_u64 << std::endl;

//     for (size_t i = 0; i < g1_count; ++i) {
//         mcl::bn::G1 affine_g1 = g1_data[i];
//         affine_g1.normalize();   

//         const mcl::bn::Fp& x = affine_g1.x;
//         const mcl::bn::Fp& y = affine_g1.y;

//         file << x << "," << y << std::endl;

//         if (!x.isValid() || !y.isValid()) {
//             std::cerr << "警告：第 " << i << " 个 G1 点坐标无效，已写入但可能无法正常解析" << std::endl;
//         }
//     }

//     file.close();
//     std::cout << "成功写入十进制文本文件: " << filename << std::endl;
//     return true;
// }
/**
 * 写入 vi(ll数组) + G1数组 到二进制文件
 * 文件格式：
 * [uint64 vi_len]
 * [vi 数据 ...]
 * [uint64 g1_count]
 * [G1_0 | G1_1 | ...]   每个 G1 = 96 bytes (BN254)
 */
// bool write_ec_data_to_file(const std::string& filename,const ll* vi,size_t vi_len,const mcl::bn::G1* g1_data,size_t g1_count)
// {
//     if (!vi || vi_len == 0 || !g1_data || g1_count == 0) {
//         std::cerr << "错误：输入参数无效" << std::endl;
//         return false;
//     }

//     std::ofstream file(filename, std::ios::binary);
//     if (!file.is_open()) {
//         std::cerr << "错误：无法打开文件 " << filename << std::endl;
//         return false;
//     }

//     /* ---------- 写 vi ---------- */
//     uint64_t vi_len_u64 = static_cast<uint64_t>(vi_len);
//     file.write(reinterpret_cast<const char*>(&vi_len_u64), sizeof(uint64_t));
//     file.write(reinterpret_cast<const char*>(vi), sizeof(ll) * vi_len);

//     /* ---------- 写 G1 元信息 ---------- */
//     uint64_t g1_count_u64 = static_cast<uint64_t>(g1_count);
//     file.write(reinterpret_cast<const char*>(&g1_count_u64), sizeof(uint64_t));

//     /* ---------- 写 G1 数据 ---------- */
//     constexpr size_t BYTES_PER_G1 = 32; // BN254 G1 固定长度
//     uint8_t buf[BYTES_PER_G1];

//     for (size_t i = 0; i < g1_count; ++i) {
//         size_t written = g1_data[i].serialize(
//             buf,
//             sizeof(buf),
//             mcl::IoSerialize
//         );

//         if (written != BYTES_PER_G1) {
//             std::cerr << "错误：第 " << i << " 个 G1 序列化失败，written="
//                       << written << std::endl;
//             file.close();
//             return false;
//         }

//         file.write(reinterpret_cast<const char*>(buf), BYTES_PER_G1);
//     }

//     file.close();
//     std::cout << "成功写入文件: " << filename << std::endl;
//     return true;
// }
bool write_ec_data_to_npy(
    const std::string& vi_file,
    const std::string& g1_file,
    const ll* vi, size_t vi_len,
    const mcl::bn::G1* g1_data, size_t g1_count
) {
    if (!vi || !g1_data) return false;


    // std::vector<uint64_t> vi_mat;
    // vi_mat.reserve(vi_len * 4);

    // size_t max_bit_width = 0;   // 最大 bit 宽度
    // size_t min_bit_width = 128; // 最小 bit 宽度（非零元素可单独统计）
    // size_t zero_count = 0;      // 0 元素个数
    // std::vector<size_t> bit_cnt(129, 0); // 统计每种 bit 宽度出现次数，范围 0~128

    // for (size_t i = 0; i < vi_len; ++i) {
    // // for (size_t i = 0; i < 32*1024; ++i) {
    //     __uint128_t x = (__uint128_t)vi[i];

    //     // 统计 bit 宽度
    //     size_t bits = 0;
    //     if (x == 0) {
    //         bits = 0;
    //         ++zero_count;
    //     } else {
    //         uint64_t hi = (uint64_t)(x >> 64);
    //         uint64_t lo = (uint64_t)x;

    //         if (hi != 0) {
    //             bits = 64 + (64 - __builtin_clzll(hi));
    //         } else {
    //             bits = 64 - __builtin_clzll(lo);
    //         }

    //         max_bit_width = std::max(max_bit_width, bits);
    //         min_bit_width = std::min(min_bit_width, bits);
    //     }

    //     bit_cnt[bits]++;

    //     vi_mat.push_back((uint64_t)x);
    //     vi_mat.push_back((uint64_t)(x >> 64));
    //     vi_mat.push_back((uint64_t)0);
    //     vi_mat.push_back((uint64_t)0);
    // }

    // std::cout << "total elements: " << vi_len << "\n";
    // std::cout << "zero elements : " << zero_count << "\n";
    // std::cout << "max bit width : " << max_bit_width << "\n";
    // if (zero_count < vi_len) {
    //     std::cout << "min nonzero bit width: " << min_bit_width << "\n";
    // }

    // // 输出每种 bit 宽度的分布
    // for (size_t b = 0; b <= 128; ++b) {
    //     if (bit_cnt[b] > 0) {
    //         std::cout << "bit width " << b << ": " << bit_cnt[b] << "\n";
    //     }
    // }
    //---------- vi: (vi_len, 2) ----------
    std::vector<uint64_t> vi_mat;
    vi_mat.reserve(vi_len * 4);
    for (size_t i = 0; i < vi_len; ++i) {
        __uint128_t x = (__uint128_t)vi[i];
        vi_mat.push_back((uint64_t)x);
        vi_mat.push_back((uint64_t)(x >> 64));
        vi_mat.push_back((uint64_t)0);
        vi_mat.push_back((uint64_t)0);        
    }

    cnpy::npy_save(vi_file, vi_mat.data(), {vi_len, 4}, "w");

    // ---------- g1: (g1_count, 4) ----------
    std::vector<uint64_t> g1_mat;
    g1_mat.reserve(g1_count * 8);
    uint8_t buf[32];
    for (size_t i = 0; i < g1_count; ++i) {
        size_t written = g1_data[i].serialize(buf, 32, mcl::IoSerialize);
        if (written != 32) {
            std::cerr << "G1 serialize failed at " << i << std::endl;
            return false;
        }
        for (int j = 0; j < 4; ++j) {
            uint64_t v;
            std::memcpy(&v, buf + j * 8, 8);
            g1_mat.push_back(v);
        }
    }
    cnpy::npy_save(g1_file, g1_mat.data(), {g1_count, 4}, "w");

    return true;
}

void prover::commitInput(const vector<G1> &gens,int thr) 
{
    int len;
    if (C.circuit[0].size != (1ULL << C.circuit[0].bit_length)) 
    {
        len=val[0].size();
        val[0].resize(1ULL << C.circuit[0].bit_length);
        for (int i = C.circuit[0].size; i < val[0].size(); ++i)
            val[0][i].clear();
    }
    
    int l=ceil(log2(val[0].size()));
    ll* vi=new ll[1<<l];
    memset(vi,0,sizeof(ll)*(1<<l));
    ll mx=-1e9,mn=1e9;
    for(int i=0;i<val[0].size();i++)
    {
        vi[i]=convert(val[0][i]);   //存储input数据的ll形式,转化为128bit
        mx=max(mx,vi[i]);
        mn=min(mn,vi[i]);
        
    }
    
    Fr* dat=new Fr[1<<l];  // 存储input数据的fr形式
    memset(dat,0,sizeof(Fr)*(1<<l));
    memcpy(dat,val[0].data(),sizeof(Fr)*val[0].size());

    // printf("vi_size=%d,gens_size=%d,llsize=%d\n",val[0].size(),gens.size(),sizeof(ll));
    // printf("vi[2]=%lld,vi[5]=%lld,vi[10]=%lld\n",vi[2],vi[5],vi[10]);
    // timer copy_timer;
    // copy_timer.start();
    //bool temp=write_ec_data_to_npy("vi_old.npy","g1.npy",vi,val[0].size(),gens.data(),gens.size());
    // copy_timer.stop();
    // printf("=============保存数据耗时: %.2f 秒！======================\n", copy_timer.elapse_sec());
    // if (temp==true){
    //     printf("=============保存数据完成！======================\n");
    // }

    G1* ret=prover_commit(vi,(G1*)gens.data(),l,thr);
    
    cc.comm=ret;
    cc.G=gens.back();
    cc.g=(G1*)gens.data();
    cc.l=l;
    cc.w=dat;
    cc.ww=vi;
    val[0].resize(len);
}

__int128 convert(Fr x)	
{	
    int sign=0;	
    Fr abs;	
    if(x.isNegative())	
    {	
        sign=1;	
        abs=-x;	
    }	
    else	
        abs=x;	

    uint8_t bf[16]={0};	 //128 bit
    int size=abs.getLittleEndian(bf,16);	
    ll V=0;	
    for(int j=size-1;j>=0;j--)	
        V=V*256+bf[j];	
    if(sign)	
        V=-V;	
    return V;	
}
