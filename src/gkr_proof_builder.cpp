//
// Created by 69029 on 3/9/2021.
//
#include "gkr_proof_builder.hpp"
#include "global_var.hpp"
#include <utils.hpp>
#include <circuit.h>
#include <iostream>
#include "generator_cache.hpp"
#include "gkr_verifier.hpp"
#include "hyrax_opening.hpp"
#include "thread_joiner.hpp"
#include "memory_diagnostics.hpp"
#include "lasso_folding_workspace.hpp"

using namespace std;
using namespace mcl::bn;
vector<F> beta_v;
static vector<F> beta_u, beta_gs;



GKRProofBuilder::GKRProofBuilder(
    prover *pr, const layeredCircuit &cir, const WitnessShape &model_shape,
    const GeneratorDomain &generator_domain, ChallengeObserver *observer,
    const std::array<std::uint8_t,32> *integration_context):
    p(pr), C(cir), transcript_(kGKRTranscriptDomain,observer,"gkr-main")
{
    if (!p) throw std::invalid_argument("GKR proof builder requires a prover");
    comm=p->cc;
    final_claim_u0.resize(C.size + 2);
    final_claim_v0.resize(C.size + 2);
    fconn_v_r_u.resize(C.size + 2);
    fconn_v_r_v.resize(C.size + 2);
    fconn_v_final_u0.resize(C.size + 2);
    fconn_v_final_v0.resize(C.size + 2);
    prover_time=builder_check_time=0;
    matrix_time=0;
    r_u.resize(C.size + 2);
    r_v.resize(C.size + 2);
    // make the prover ready
    p->init();
    if (comm.l<0 || comm.l>30)
        throw std::invalid_argument("invalid GKR input log size");
    const std::size_t commitment_count=static_cast<std::size_t>(1)<<(comm.l/2);
    if (integration_context) public_statement_.integration_context=*integration_context;
    public_statement_.model_shape=model_shape;
    public_statement_.val0_log_size=comm.l;
    public_statement_.val0_commitment.assign(
        comm.comm,comm.comm+commitment_count);
    public_statement_.val0_generator_domain=generator_domain;
    const auto trusted_fingerprint=fingerprintCircuit(C);
    public_statement_.circuit_fingerprint=trusted_fingerprint;
    validateGKRPublicStatement(C,public_statement_,trusted_fingerprint);
    transcript_=initializeGKRTranscript(
        public_statement_,trusted_fingerprint,observer,"gkr-main");
}

F GKRProofBuilder::challenge(const std::string &label) {
    return transcript_.challenge(label);
}

void GKRProofBuilder::appendQuadratic(const std::string &label,
                               const quadratic_poly &poly) {
    transcript_.appendFr(label+".a", poly.a);
    transcript_.appendFr(label+".b", poly.b);
    transcript_.appendFr(label+".c", poly.c);
}

F GKRProofBuilder::getFinalValue(const F &claim_u0, const F &claim_u1, const F &claim_v0, const F &claim_v1) {
    auto test_value = bin_value[0] * (claim_u0 * claim_v0)
                      + bin_value[1] * (claim_u1 * claim_v1)
                      + bin_value[2] * (claim_u1 * claim_v0)
                      + uni_value[0] * claim_u0
                      + uni_value[1] * claim_u1;

    return test_value;
}

void GKRProofBuilder::betaInitPhase1(u32 depth, const F &alpha, const F &beta, const vector<F>::const_iterator &r_0, const vector<F>::const_iterator &r_1, const F &relu_rou) {
    i8 bl = C.circuit[depth].bit_length;
    i8 fft_bl = C.circuit[depth].fft_bit_length;
    i8 fft_blh = C.circuit[depth].fft_bit_length - 1;
    i8 cnt_bl = bl - fft_bl, cnt_bl2 = C.circuit[depth].max_bl_u - fft_bl;


        
            beta_g.resize(1ULL << bl);
            initBetaTable(beta_g, C.circuit[depth].bit_length, r_0, r_1, alpha, beta );
            if (C.circuit[depth].zero_start_id < C.circuit[depth].size)
                for (u32 g = C.circuit[depth].zero_start_id; g < 1ULL << C.circuit[depth].bit_length; ++g)
                    beta_g[g] = beta_g[g] * relu_rou;
            beta_u.resize(1ULL << C.circuit[depth].max_bl_u);
            initBetaTable(beta_u, C.circuit[depth].max_bl_u, r_u[depth].begin(), F_ONE);
    
}

void GKRProofBuilder::betaInitPhase2(u32 depth) {
    beta_v.resize(1ULL << C.circuit[depth].max_bl_v);
    initBetaTable(beta_v, C.circuit[depth].max_bl_v, r_v[depth].begin(), F_ONE);
}

static ThreadSafeQueue<int> workerq,endq;
void pred1_worker(const vector<uniGate> &beg, vector<F>& uni_value,vector<F>& beta_g, vector<F>& beta_u,int*&L,int*&R) //F*& uni_value, layer &cur_layer,
{
    int idx;
    while (true)
    {
        bool ret=workerq.TryPop(idx);
        if(ret==false)
            return;
        int l=L[idx],r=R[idx];
        for (auto gate=beg.begin()+l;gate!=beg.begin()+r;gate++) 
        {
            bool idx = gate->lu;
            uni_value[idx]+= beta_g[gate->g] * beta_u[gate->u] * gate->sc;
        }
        endq.Push(idx);
    }
}
void pred2_worker(const vector<binGate> &beg, vector<F>& bin_value,vector<F>& beta_g, vector<F>& beta_u,int*&L,int*&R) //F*& uni_value, layer &cur_layer,
{
    int idx;
    while (true)
    {
        bool ret=workerq.TryPop(idx);
        if(ret==false)
            return;
        int l=L[idx],r=R[idx];
        for (auto gate=beg.begin()+l;gate!=beg.begin()+r;gate++) 
        {
            bin_value[gate->l] += beta_g[gate->g] * beta_u[gate->u] * beta_v[gate->v] * gate->sc;
        }
        endq.Push(idx);
    }
}

void GKRProofBuilder::predicatePhase1(u32 layer_id)
{    
    auto &cur_layer = C.circuit[layer_id];
    uni_value[0].clear();
    uni_value[1].clear();
    vector<F>  univ[40];
    for(int i=0;i<32;i++)
    {
            univ[i].emplace_back(F(0));
            univ[i].emplace_back(F(0));
    }
    if(C.circuit[layer_id].uni_interval.size()>=2)
    {
        const int thd=32;
        int *L=new int [C.circuit[layer_id].uni_interval.size()],*R=new int [C.circuit[layer_id].uni_interval.size()];
        for (u64 j = 0; j <C.circuit[layer_id].uni_interval.size(); ++j) 
        {
                workerq.Push(j);
                L[j]=cur_layer.uni_interval[j].first;
                R[j]=cur_layer.uni_interval[j].second;
        }
            // Joined (not detached) workers: a late-starting detached thread
            // could otherwise pop the next call's tasks with stale references.
            std::vector<std::thread> workers;
            zkgpt::ThreadJoiner joiner(workers);
            for(int i=0;i<thd;i++)
                workers.emplace_back(pred1_worker, std::cref(cur_layer.uni_gates),std::ref(univ[i]),std::ref(beta_g),std::ref(beta_u),std::ref(L),std::ref(R));
            joiner.join();
            endq.Clear();
            delete[] L;
            delete[] R;
            for(int i=0;i<thd;i++)
            {
                uni_value[0]+=univ[i][0];
                uni_value[1]+=univ[i][1];
            }
    }
    else for (auto &gate: cur_layer.uni_gates) 
    {
        bool idx = gate.lu;
        uni_value[idx]+= beta_g[gate.g] * beta_u[gate.u] * gate.sc;
    }
    bin_value[0] = bin_value[1] = bin_value[2] = F_ZERO;
}

void GKRProofBuilder::predicatePhase2(u32 layer_id)
{
    uni_value[0] = uni_value[0] * beta_v[0];
    uni_value[1] = uni_value[1] * beta_v[0];

    auto &cur_layer = C.circuit[layer_id];
    vector<F>  binv[40];
    for(int i=0;i<32;i++)
    {
            binv[i].emplace_back(F(0));
            binv[i].emplace_back(F(0));
            binv[i].emplace_back(F(0));
    }
    if(C.circuit[layer_id].bin_interval.size()>=2)
    {
        const int thd=32;
        int *L=new int [C.circuit[layer_id].bin_interval.size()],*R=new int [C.circuit[layer_id].bin_interval.size()];
        for (u64 j = 0; j <C.circuit[layer_id].bin_interval.size(); ++j) 
        {
                workerq.Push(j);
                L[j]=cur_layer.bin_interval[j].first;
                R[j]=cur_layer.bin_interval[j].second;
        }
            // Joined (not detached) workers: a late-starting detached thread
            // could otherwise pop the next call's tasks with stale references.
            std::vector<std::thread> workers;
            zkgpt::ThreadJoiner joiner(workers);
            for(int i=0;i<thd;i++)
                workers.emplace_back(pred2_worker, std::cref(cur_layer.bin_gates),std::ref(binv[i]),std::ref(beta_g),std::ref(beta_u),std::ref(L),std::ref(R));
            joiner.join();
            endq.Clear();
            delete[] L;
            delete[] R;
            for(int i=0;i<thd;i++)
            {
                bin_value[0]+=binv[i][0];
                bin_value[1]+=binv[i][1];
                bin_value[2]+=binv[i][2];
            }
    }
    else for (auto &gate: cur_layer.bin_gates)
        bin_value[gate.l] += beta_g[gate.g] * beta_u[gate.u] * beta_v[gate.v] * gate.sc;
}

void GKRProofBuilder::build()
{
    cout<<"Circuit Initiation finished"<<endl;
    cout<<"++++++++++++++++++++++Proving Service started+++++++++++++++++++++++"<<endl;
    p->logWorkspaceMemory("gkr_before_layers");
    if (!buildLayerProofs())
        throw std::runtime_error("GKR layer proof construction failed");
    p->logWorkspaceMemory("gkr_after_layers");
    if (!buildLassoProof())
        throw std::runtime_error("GKR Lasso proof construction failed");
    p->logWorkspaceMemory("gkr_after_lasso");
    // Input opening uses comm.w, commitments and r_u[0], not the Lasso
    // coefficient table. Release it before Hyrax copies/folds the witness.
    p->releaseLassoWorkspace();
    p->logWorkspaceMemory("gkr_after_lasso_release");
    if (!buildInputOpeningProof())
        throw std::runtime_error("GKR input opening construction failed");
    p->logWorkspaceMemory("gkr_after_input_opening");
    gkr_proof_.transcript_binding=challenge("gkr.proof.final");
    cout<<"=========================Our results:================================"<<endl;
    cout<<"Matrix multiplication Prover time: "<<matrix_time<<"s"<<endl;
    cout<<"Total Prover time(GKR+range):"<<prover_time + range_prover_time<<"="<<prover_time<<"+"<<range_prover_time<<"s"<<endl;
    cout<<"GKR proof-builder local consistency-check time: "
        <<builder_check_time<<"s"<<endl;
    cout<<"Proof size(old): "<<(double)(p->proof_size)/1024.0<<"KB"<<endl;
}

Fr * init_book_keeping(int m,int n,vector<Fr> &vec,int offset, vector<Fr> & ra)
{
    if (ra.size()!=static_cast<size_t>(m+n))
        throw std::length_error("init_book_keeping random point has wrong size");
    Fr* pa[30];
    for(int j=0;j<=n;j++)
    {
        pa[j]=new Fr[1<<(m+n-j)];
    }
    int t=min((1<<(m+n)),(int)vec.size()-offset);
    memset(pa[0]+t,0,sizeof(Fr)*((1<<(m+n))-t));
    
    for(int j=0;j<t;j++)
        pa[0][j]=vec[j+offset];
    for(int j=1;j<=n;j++)
    {
        for(int kp=0;kp<(1<<(m+n-j));kp++)
        {
            pa[j][kp]=pa[j-1][kp+(1<<(m+n-j))]*ra[m+n-j]+pa[j-1][kp]*(1-ra[m+n-j]);
        }
    }
    for(int j=0;j<n;j++)
    {
        delete [] pa[j];
    }
    return pa[n];
}
Fr fm[1<<8][65536];
Fr * init_book_keeping_fast(int m,int n,int* vec,vector<Fr> & ra)
{
    if (ra.size()!=static_cast<size_t>(m+n))
        throw std::length_error("init_book_keeping_fast random point has wrong size");
    // fm caches small multiples of eq2 for 2^(n/2) <= 256 rows, and the split
    // into eq1 x eq2 needs an even number of column variables.
    if (n%2!=0 || n/2>8)
        throw std::length_error("init_book_keeping_fast unsupported column bits");
    Fr* ret=new Fr[1<<m];
    vector<Fr> eq1(1<<(n/2)), eq2(1<<(n/2));
    for(int j=0;j<(1<<(n/2));j++)
    {
        eq1[j]=1;
        for(int i=0;i<n/2;i++)
        {
            if(j&(1<<(n/2-1-i)))
                eq1[j]*=ra[m+n-1-i];
            else
                eq1[j]*=1-ra[m+n-1-i];
        }
    }
    for(int j=0;j<(1<<(n/2));j++)
    {
        eq2[j]=1;
        for(int i=0;i<n/2;i++)
        {
            if(j&(1<<(n/2-1-i)))
                eq2[j]*=ra[m+n/2-1-i];
            else
                eq2[j]*=1-ra[m+n/2-1-i];
        }
    }
    std::vector<std::thread> threads1,threads2;
    int num_threads=32;
    // Round up so small sizes still get work and no remainder is dropped.
    int chunk_size = ((1<<(n/2))+num_threads-1)/num_threads;
    auto worker1 = [&](int start_id, int end_id) 
    {
        for(int R=start_id;R<end_id;R++)
        {
            fm[R][0]=0;
            fm[R][1]=eq2[R];
            for(int i=2;i<(1<<16);i++)
                fm[R][i]=fm[R][i-1]+eq2[R];
        }
    };
    for (int t = 0; t < num_threads; t++) 
    {
        int start_col = min(t * chunk_size, (1<<(n/2)));
        int end_col = min(start_col + chunk_size, (1<<(n/2)));
        threads1.emplace_back(worker1, start_col, end_col);
    }
    for (auto &th : threads1) 
    {
        if (th.joinable())
            th.join();
    }
    auto worker2 = [&](int start_id, int end_id) 
    {
        for(int kp=start_id;kp<end_id;kp++)
        {
            ret[kp]=0;
            Fr sum=0;
            for(int L=0;L<(1<<(n/2));L++)
            {
                Fr sumL=0;
                for(int R=0;R<(1<<(n/2));R++)
                {
                    int j=((L<<(n/2))+R)<<m;
                    int w = vec[kp+j];
                    if(w>0) {
                        if(w < (1<<16))
                            sumL+=fm[R][w];
                        else
                            sumL+=Fr(w)*eq2[R];
                    } else if(w<0) {
                        int abs_w = -w;
                        if(abs_w < (1<<16))
                            sumL-=fm[R][abs_w];
                        else
                            sumL+=Fr(w)*eq2[R];
                    }
                }
                sum+=eq1[L]*sumL;
            }
            ret[kp]=sum;
        }
    };
    chunk_size = ((1<<m)+num_threads-1)/num_threads;
    for (int t = 0; t < num_threads; t++) 
    {
        int start_col = min(t * chunk_size, (1<<m));
        int end_col = min(start_col + chunk_size, (1<<m));
        threads2.emplace_back(worker2, start_col, end_col);
    }
    for (auto &th : threads2) 
    {
        if (th.joinable())
            th.join();
    }
    return ret;
}

void sc_last_worker(Fr& A, Fr&B,Fr&C,Fr*& read_1,Fr*& read_2,Fr*& write_1,Fr*& write_2, int*& L,int*& R,Fr ran)
{
    int idx;
    while (true)
    {
        bool ret=workerq.TryPop(idx);
        if(ret==false)
            return;
        int l=L[idx],r=R[idx];
        for(int kp=l;kp<r;kp++)
        {
            zkgpt::lassoFoldEntry(read_1,read_2,write_1,write_2,kp,ran);
                if(kp&1)
                {
                    bool az=write_2[kp-1].isZero(),bz=write_2[kp].isZero();
                    if(az && bz)
                        continue;
                    //if(!az)
                        A+=write_1[kp-1]*write_2[kp-1];
                    //if(!bz)
                        B+=write_1[kp]*write_2[kp];
                    C+=(write_1[kp]+write_1[kp]-write_1[kp-1])*(write_2[kp]+write_2[kp]-write_2[kp-1]);
                }
        }
        endq.Push(idx);
    }
}

void sc_last_worker_first(Fr& A, Fr&B,Fr&C,Fr*& read_1,Fr*& read_2, int*& L,int*& R)
{
    int idx;
    while (true)
    {
        bool ret=workerq.TryPop(idx);
        if(ret==false)
            return;
        int l=L[idx],r=R[idx];
        for(int k=l;k<r;k++)
        {
            if(k&1)
            {
                bool a_z=read_2[k].isZero(),b_z=read_2[k-1].isZero();
                if(a_z && b_z)
                    continue;
                B+=read_1[k]*read_2[k];
                C+=(read_1[k]+read_1[k]-read_1[k-1])*(read_2[k]+read_2[k]-read_2[k-1]); 
            }
            else if(!read_2[k].isZero())
                A+=read_1[k]*read_2[k];  
        }
        endq.Push(idx);
    }
}


// Matrix-product sumcheck. Every polynomial is absorbed before its challenge;
// random_points is both the proof evaluation point and the point used by the
// following GKR layer.
pair<Fr,Fr> sum_check_product(
    Fr *f, Fr *g, int rounds, Fr initial_claim,
    vector<Fr> &random_points, Transcript &transcript,
    const std::string &label, vector<GKRQuadraticRound> *proof_rounds)
{
    if (rounds<0 || rounds>30)
        throw std::invalid_argument("matrix sumcheck round count is invalid");
    const std::size_t value_count=static_cast<std::size_t>(1)<<rounds;
    vector<Fr> folded_f(f, f+value_count);
    vector<Fr> folded_g(g, g+value_count);
    random_points.resize(rounds);
    Fr claim=initial_claim;
    std::size_t active=value_count;
    for (int round=0;round<rounds;++round) {
        Fr a=0,b=0,c=0;
        for (std::size_t i=0;i<active;i+=2) {
            const Fr delta_f=folded_f[i+1]-folded_f[i];
            const Fr delta_g=folded_g[i+1]-folded_g[i];
            a+=delta_f*delta_g;
            b+=folded_f[i]*delta_g+folded_g[i]*delta_f;
            c+=folded_f[i]*folded_g[i];
        }
        const quadratic_poly polynomial(a,b,c);
        if (polynomial.eval(Fr(0))+polynomial.eval(Fr(1))!=claim)
            throw std::runtime_error("matrix sumcheck round claim failed");
        transcript.appendFr(label+".a", a);
        transcript.appendFr(label+".b", b);
        transcript.appendFr(label+".c", c);
        if (proof_rounds) proof_rounds->push_back({a,b,c});
        const Fr random=transcript.challenge(label+".challenge");
        random_points[round]=random;
        claim=polynomial.eval(random);
        for (std::size_t i=0;i<active/2;++i) {
            folded_f[i]=folded_f[2*i]+random*
                (folded_f[2*i+1]-folded_f[2*i]);
            folded_g[i]=folded_g[2*i]+random*
                (folded_g[2*i+1]-folded_g[2*i]);
        }
        active/=2;
    }
    if (active!=1 || folded_f[0]*folded_g[0]!=claim)
        throw std::runtime_error("matrix sumcheck final claim failed");
    transcript.appendFr(label+".final_f", folded_f[0]);
    transcript.appendFr(label+".final_g", folded_g[0]);
    return make_pair(folded_f[0], folded_g[0]);
}


bool GKRProofBuilder::buildLayerProofs()
{
    if (C.size < 2 || C.circuit.size() != C.size)
        throw std::runtime_error("verifyGKR: invalid circuit size");
    F alpha = F_ONE, beta = F_ZERO, relu_rou, final_claim_u1, final_claim_v1;
    const layer &output_layer = C.circuit.at(C.size - 1);
    r_u.at(C.size).resize(output_layer.bit_length);
    for (u32 i = 0; i < static_cast<u32>(output_layer.bit_length); ++i)
        r_u.at(C.size).at(i)=challenge("gkr.output.point");
    vector<F>::const_iterator r_0 = r_u.at(C.size).begin();
    vector<F>::const_iterator r_1;

    auto previousSum = p->Vres(r_0, output_layer.size,
                               output_layer.bit_length, C.size - 1);
    transcript_.appendFr("gkr.output.evaluation", previousSum);
    gkr_proof_.output_evaluation=previousSum;
    public_statement_.output_evaluation=previousSum;
    gkr_proof_.layers.clear();
    gkr_proof_.layers.reserve(C.size-1);
    timer ptimer,vtimer;
    timer mat_timer; // 矩阵乘的时间
    ptimer.start();
    p -> sumcheckInitAll(r_0);
    ptimer.stop();
    prover_time+=ptimer.elapse_sec();
    int fc_cnt=0,now_fc_id;
    Fr prev_u1=0,prev_v1=0;
    for (u32 i = C.size; i-- > 1; )
    {
        if(C.circuit.at(i).ty==layerType::FCONN)
            fc_cnt++;
    } 
    
    timer init,sc;
    double init_t=0,sc_t=0;
    double matrix_tt_time=0;
    for (u32 i = C.size; i-- > 1; )
    {
        timer tmp;
        tmp.start();
        auto &cur = C.circuit.at(i);
        GKRLayerProof layer_proof;
        layer_proof.layer_index=i;
        layer_proof.layer_type=static_cast<std::uint32_t>(cur.ty);
        final_claim_v0.at(i).clear();

        ptimer.start();
        p->sumcheckInit(alpha, beta);
        ptimer.stop();
        prover_time+=ptimer.elapse_sec();
        
        // phase 1
        if (cur.zero_start_id < cur.size)
        {
            relu_rou=challenge("gkr.relu.zero_scale");
        }
        else
            relu_rou = F_ONE;
        F previousRandom = F_ZERO;
        // 全连接层使用高效的验证
        if(cur.ty==layerType::FCONN)   //substitute matrix with thaler13
        {
            now_fc_id=--fc_cnt;
            int n,m,k;
            n=ceilPow2BitLength(p->fc_input_row[now_fc_id]);
            k=ceilPow2BitLength(p->fc_col[now_fc_id]);
            m=ceilPow2BitLength(p->fc_row[now_fc_id]);
            const std::size_t output_point_size=
                static_cast<std::size_t>(n+k);
            if (r_u.at(i+1).size()<output_point_size ||
                r_v.at(i+1).size()<output_point_size)
                throw std::runtime_error(
                    "FCONN requires both parent output evaluation points");
            const auto initialize_matrix_points=[&](
                const vector<F> &output_point, vector<F> &x_point,
                vector<F> &w_point) {
                x_point.resize(m+n);
                w_point.resize(m+k);
                for (int j=0;j<n;++j)
                    x_point.at(m+j)=output_point.at(k+j);
                for (int j=0;j<k;++j)
                    w_point.at(m+j)=output_point.at(j);
            };
            initialize_matrix_points(r_u.at(i+1),r_u.at(i),r_v.at(i));
            initialize_matrix_points(
                r_v.at(i+1),fconn_v_r_u.at(i),fconn_v_r_v.at(i));
            prev_u1=final_claim_u1;
            prev_v1=final_claim_v1;
            previousRandom = F_ZERO;
            ptimer.start();
            mat_timer.start();
            Fr* pa=init_book_keeping(m,n,p->val[0],p->fc_input_id[now_fc_id],r_u[i]);
            Fr* pb=init_book_keeping_fast(m,k,p->mat_val[now_fc_id],r_v[i]);
            vector<Fr> matrix_point;
            pair<Fr,Fr> oracle=sum_check_product(
                pa, pb, m, prev_u1, matrix_point, transcript_,
                "gkr.matrix.u."+std::to_string(i),
                &layer_proof.matrix_rounds);
            delete[] pa;
            delete[] pb;
            for(int j=0;j<m;++j) {
                r_u.at(i).at(j)=matrix_point.at(j);
                r_v.at(i).at(j)=matrix_point.at(j);
            }
            pa=init_book_keeping(
                m,n,p->val[0],p->fc_input_id[now_fc_id],
                fconn_v_r_u.at(i));
            pb=init_book_keeping_fast(
                m,k,p->mat_val[now_fc_id],fconn_v_r_v.at(i));
            vector<Fr> matrix_v_point;
            const pair<Fr,Fr> oracle_v=sum_check_product(
                pa,pb,m,prev_v1,matrix_v_point,transcript_,
                "gkr.matrix.v."+std::to_string(i),
                &layer_proof.matrix_v_rounds);
            delete[] pa;
            delete[] pb;
            for (int j=0;j<m;++j) {
                fconn_v_r_u.at(i).at(j)=matrix_v_point.at(j);
                fconn_v_r_v.at(i).at(j)=matrix_v_point.at(j);
            }
            mat_timer.stop();
            ptimer.stop();
            matrix_time+=mat_timer.elapse_sec();
            prover_time+=ptimer.elapse_sec();    
            final_claim_u0[i]=oracle.first;
            fconn_v_final_u0[i]=oracle_v.first;
            fconn_v_final_v0[i]=oracle_v.second;
            const auto mapped_evaluation=[&](
                const vector<u32> &mapping, std::size_t count,
                const vector<F> &point) {
                if (count>mapping.size() ||
                    (std::size_t{1}<<point.size())<count)
                    throw std::runtime_error(
                        "FCONN committed-input mapping shape mismatch");
                vector<F> values(std::size_t{1}<<point.size(),F_ZERO);
                for (std::size_t j=0;j<count;++j)
                    values[j]=p->val[0].at(mapping[j]);
                std::size_t active=values.size();
                for (const auto &coordinate : point) {
                    for (std::size_t j=0;j<active/2;++j)
                        values[j]=values[2*j]+coordinate*
                            (values[2*j+1]-values[2*j]);
                    active/=2;
                }
                return values[0];
            };
            const auto &matrix_layer=C.circuit.at(i);
            if (mapped_evaluation(
                    matrix_layer.ori_id_u,matrix_layer.size_u[0],r_u.at(i))!=
                    oracle.first ||
                mapped_evaluation(
                    matrix_layer.ori_id_v,matrix_layer.size_v[0],r_v.at(i))!=
                    oracle.second ||
                mapped_evaluation(
                    matrix_layer.ori_id_u,matrix_layer.size_u[0],
                    fconn_v_r_u.at(i))!=oracle_v.first ||
                mapped_evaluation(
                    matrix_layer.ori_id_v,matrix_layer.size_v[0],
                    fconn_v_r_v.at(i))!=oracle_v.second)
                throw std::runtime_error(
                    "FCONN matrix endpoint is inconsistent with val[0] mapping");
            final_claim_u1=0;
            final_claim_v0[i]=oracle.second;
            final_claim_v1=0;
            layer_proof.matrix_v_final_claim_u0=oracle_v.first;
            layer_proof.matrix_v_final_claim_v0=oracle_v.second;
        }
        // 普通层分为两个阶段
        else
        {
            timer normal_timer;

            normal_timer.start();
            ptimer.start();
            p->sumcheckInitPhase1(relu_rou);
            ptimer.stop();
            prover_time+=ptimer.elapse_sec();            
            r_u.at(i).resize(cur.max_bl_u);
            sc.start();
            for (u32 j = 0; j < static_cast<u32>(cur.max_bl_u); ++j) 
            {
                F cur_claim, nxt_claim;
                ptimer.start();
                quadratic_poly poly = p->sumcheckUpdate1(previousRandom);
                ptimer.stop();
                prover_time+=ptimer.elapse_sec();     

                vtimer.start();
                appendQuadratic("gkr.phase1", poly);
                layer_proof.phase1_rounds.push_back({poly.a,poly.b,poly.c});
                r_u.at(i).at(j)=challenge("gkr.phase1.challenge");
                cur_claim = poly.eval(F_ZERO) + poly.eval(F_ONE);
                nxt_claim = poly.eval(r_u.at(i).at(j));

                if (cur_claim != previousSum) 
                {
                    cerr << cur_claim << ' ' << previousSum << endl;
                    fprintf(stderr, "Verification fail, phase1, circuit %u, current bit %u\n", i, j);
                    return false;
                }
                vtimer.stop();
                builder_check_time+=vtimer.elapse_sec();
                previousRandom = r_u.at(i).at(j);
                previousSum = nxt_claim;
            }
            sc.stop();
            sc_t+=sc.elapse_sec();
            prev_u1=final_claim_u1;
            ptimer.start();
            p->sumcheckFinalize1(previousRandom, final_claim_u0[i], final_claim_u1);
            betaInitPhase1(i, alpha, beta, r_0, r_1, relu_rou);
            predicatePhase1(i);
            ptimer.stop();
            prover_time+=ptimer.elapse_sec();     
            // 阶段二
            if (cur.need_phase2) 
            {
                timer normal_timer2;
                normal_timer2.start();
                r_v.at(i).resize(cur.max_bl_v);
                ptimer.start();
                p->sumcheckInitPhase2();
                ptimer.stop();
                prover_time+=ptimer.elapse_sec();     
                previousRandom = F_ZERO;
                sc.start();
                for (u32 j = 0; j < cur.max_bl_v; ++j) 
                {
                        ptimer.start();
                        quadratic_poly poly = p->sumcheckUpdate2(previousRandom);
                        ptimer.stop();
                        prover_time+=ptimer.elapse_sec();     
                        vtimer.start();
                        appendQuadratic("gkr.phase2", poly);
                        layer_proof.phase2_rounds.push_back(
                            {poly.a,poly.b,poly.c});
                        r_v.at(i).at(j)=challenge("gkr.phase2.challenge");
                        if (poly.eval(F_ZERO) + poly.eval(F_ONE) != previousSum)
                        {
                            fprintf(stderr, "Verification fail, phase2, circuit level %u, current bit %u, total is %d\n", i, j,
                                    cur.max_bl_v);
                            return false;
                        }
                        vtimer.stop();
                        builder_check_time+=vtimer.elapse_sec();
                        previousRandom = r_v.at(i).at(j);
                        previousSum = poly.eval(previousRandom);
                }
                sc.stop();
                sc_t+=sc.elapse_sec();
                prev_v1=final_claim_v1;
                ptimer.start();
                p->sumcheckFinalize2(previousRandom, final_claim_v0[i], final_claim_v1);
                betaInitPhase2(i);
                predicatePhase2(i);
                ptimer.stop();
                prover_time+=ptimer.elapse_sec();     
            }
            else
            {
                final_claim_v1.clear();
            }

        }
        // 最终值验证
        vtimer.start();
        if (cur.ty!=layerType::FCONN )  // for thaler13 no need to check this
        {
            F test_value = getFinalValue(final_claim_u0[i], final_claim_u1, final_claim_v0[i], final_claim_v1);
            if(previousSum != test_value)
            {
                std::cerr << "Verification fail, final claim, circuit level "
                          << i << ": expected " << test_value
                          << ", got " << previousSum << std::endl;
                return false;
            }
        }
        
        transcript_.appendFr("gkr.layer.final_u0", final_claim_u0[i]);
        transcript_.appendFr("gkr.layer.final_u1", final_claim_u1);
        transcript_.appendFr("gkr.layer.final_v0", final_claim_v0[i]);
        transcript_.appendFr("gkr.layer.final_v1", final_claim_v1);
        layer_proof.final_claim_u0=final_claim_u0[i];
        layer_proof.final_claim_u1=final_claim_u1;
        layer_proof.final_claim_v0=final_claim_v0[i];
        layer_proof.final_claim_v1=final_claim_v1;
        gkr_proof_.layers.push_back(std::move(layer_proof));
        if (~cur.bit_length_u[1])
            alpha=challenge("gkr.layer.alpha");
        else 
            alpha.clear();
        if ((~cur.bit_length_v[1]) || cur.ty == layerType::FFT)
            beta=challenge("gkr.layer.beta");
        else 
            beta.clear();
        previousSum = alpha * final_claim_u1 + beta * final_claim_v1;
        vtimer.stop();
        builder_check_time+=vtimer.elapse_sec();

        r_0 = r_u[i].begin();
        r_1 = r_v[i].begin();
        beta_u.clear();
        beta_v.clear();
        tmp.stop();
    }

    
    return true;
}

// 协议最后一步
bool GKRProofBuilder::buildLassoProof()
{
    if (comm.w==nullptr)
        throw std::runtime_error("Lasso committed padded witness is unavailable");
    gkr_proof_.lasso=GKRLassoProof{};
    timer ptimer,vtimer;
    auto &cur = C.circuit[0];

    vector<F> sig_u(C.size - 1);
    for (u32 i = 0; i < C.size - 1; ++i) 
        sig_u.at(i)=challenge("lasso.sigma_u");
    vector<F> sig_v(C.size - 1);
    for (u32 i = 0; i < C.size - 1; ++i) 
        sig_v.at(i)=challenge("lasso.sigma_v");
    vector<F> sig_fconn_v_u(C.size - 1,F_ZERO);
    vector<F> sig_fconn_v_v(C.size - 1,F_ZERO);
    for (u32 i=1;i<C.size;++i) {
        if (C.circuit.at(i).ty!=layerType::FCONN) continue;
        sig_fconn_v_u.at(i-1)=challenge("lasso.fconn_v.sigma_u");
        sig_fconn_v_v.at(i-1)=challenge("lasso.fconn_v.sigma_v");
    }
    r_u.at(0).resize(cur.bit_length);
    auto r_0 = r_u.at(0).begin();
    
    F previousSum = F_ZERO;
    for (u32 i = 1; i < C.size; ++i) 
    {
        if (~C.circuit.at(i).bit_length_u[0])
            previousSum += sig_u.at(i - 1) * final_claim_u0.at(i);
        if (~C.circuit.at(i).bit_length_v[0])
            previousSum += sig_v.at(i - 1) * final_claim_v0.at(i);
        if (C.circuit.at(i).ty==layerType::FCONN) {
            previousSum += sig_fconn_v_u.at(i-1) *
                fconn_v_final_u0.at(i);
            previousSum += sig_fconn_v_v.at(i-1) *
                fconn_v_final_v0.at(i);
        }
    }

    ptimer.start();
    p->sumcheckLassoInit(sig_u, sig_v,r_u,r_v);
    // Add the two evaluations produced by every second FCONN matrix
    // sumcheck to the same committed-input Lasso relation.  Independent
    // coefficients prevent cancellation with either the first matrix claim
    // or the other second-point oracle.
    for (u32 i=1;i<C.size;++i) {
        const auto &layer=C.circuit.at(i);
        if (layer.ty!=layerType::FCONN) continue;
        if (layer.bit_length_u[0]<0 || layer.bit_length_v[0]<0 ||
            fconn_v_r_u.at(i).size()<
                static_cast<std::size_t>(layer.bit_length_u[0]) ||
            fconn_v_r_v.at(i).size()<
                static_cast<std::size_t>(layer.bit_length_v[0]))
            throw std::runtime_error(
                "FCONN second-point Lasso metadata is invalid");
        beta_g.resize(std::size_t{1}<<layer.bit_length_u[0]);
        initBetaTable(beta_g,layer.bit_length_u[0],
                      fconn_v_r_u.at(i).begin(),sig_fconn_v_u.at(i-1),32);
        for (u32 j=0;j<layer.size_u[0];++j)
            p->lasso_mult_v.at(layer.ori_id_u.at(j))+=beta_g.at(j);
        beta_g.resize(std::size_t{1}<<layer.bit_length_v[0]);
        initBetaTable(beta_g,layer.bit_length_v[0],
                      fconn_v_r_v.at(i).begin(),sig_fconn_v_v.at(i-1),32);
        for (u32 j=0;j<layer.size_v[0];++j)
            p->lasso_mult_v.at(layer.ori_id_v.at(j))+=beta_g.at(j);
    }
    F lasso_initial_check=F_ZERO;
    if (p->lasso_mult_v.size()<p->val[0].size())
        throw std::runtime_error("Lasso coefficient table is shorter than val[0]");
    for (std::size_t i=0;i<p->val[0].size();++i)
        lasso_initial_check+=p->lasso_mult_v[i]*p->val[0][i];
    if (lasso_initial_check!=previousSum)
        throw std::runtime_error(
            "Lasso initial claim does not match committed-input evaluations");
    ptimer.stop();
    prover_time+=ptimer.elapse_sec();

    F previousRandom = F_ZERO;
    int n=C.circuit[0].bit_length;
    zkgpt::LassoFoldingWorkspace<Fr> folding(n);
    using namespace zkgpt::memory_diagnostics;
    logProcess("lasso_before_scratch");

    const __int128 FILTER=65535,NEG_FILTER=-65535;
    int *L=new int[1<<10],*R=new int[1<<10]; // 任务区间数组，用于多线程任务划分
    ptimer.start();
    // n是输入层的比特长度
    for(int i=0;i<=n;i++)
    {
        Fr A=0,B=0,C=0;
        const std::string memory_stage="lasso_fold_"+std::to_string(i);
        if (i>0) {
            logProcess(memory_stage+"_before_allocation");
            folding.beginRound(i); // All preceding workers have joined.
            logAllocation(memory_stage,"pb1",folding.outputLength(),sizeof(Fr));
            logAllocation(memory_stage,"pb2",folding.outputLength(),sizeof(Fr));
            logScalar(memory_stage,"input_length",folding.inputLength());
            logScalar(memory_stage,"output_length",folding.outputLength());
            logScalar(memory_stage,"combined_capacity_elements",folding.capacity());
            logProcess(memory_stage+"_after_allocation");
        }
        if(i==0)
        {
            Fr* read_1=p->lasso_mult_v.data(),*read_2=comm.w;
            const int k=10;
            const int thread_n=32; 
            int total_work=1<<n;
            int task_cnt=0;
            const int chunk_size=(total_work+(1<<k)-1)>>k;
            // 划分任务区间：将2^n个任务划分为2^10个任务块
            for (u64 j = 0; j < (1<<k); ++j) 
            {
                L[j]=chunk_size*j;
                if(L[j]>=p->total_size[1])
                    break;
                R[j]=std::min(total_work,chunk_size*static_cast<int>(1+j));
                task_cnt++;
                workerq.Push(j);
            }
            Fr Aa[40],Bb[40],Cc[40];
            memset(Aa,0,sizeof(Aa));
            memset(Bb,0,sizeof(Bb));
            memset(Cc,0,sizeof(Cc));
            // 启动32个线程并行计算
            // Joined (not detached) workers: a late-starting detached thread
            // could otherwise pop the next call's tasks with stale references.
            std::vector<std::thread> workers;
            zkgpt::ThreadJoiner joiner(workers);
            for(int j=0;j<thread_n;j++)
                workers.emplace_back(sc_last_worker_first,std::ref(Aa[j]),std::ref(Bb[j]),std::ref(Cc[j]),std::ref(read_1),std::ref(read_2),std::ref(L),std::ref(R));
            joiner.join();
            endq.Clear();
            //Fr ax=0,bx=0,cx=0;
            // 合并线程结果
            for(int j=0;j<thread_n;j++)
            {
                A+=Aa[j];
                B+=Bb[j];
                C+=Cc[j];
            }
        }
        else if(i<=9 && n-i>=11)
        {
            timer tt;
            tt.start();
            Fr* read_1=p->lasso_mult_v.data(),*read_2=comm.w,*write_1=folding.output1(),*write_2=folding.output2();
            if(i!=1)    
                read_1=folding.input1(),read_2=folding.input2();
            const int k=10;
            const int thread_n=32; 
            int total_work=1<<(n-i);
            int taskcnt=0;
            for (u64 j = 0; j < (1<<k); ++j) 
            {
                L[j]=(total_work>>k)*j;
                R[j]=(total_work>>k)*(1+j);
                if((1<<i)*L[j]>=p->total_size[1])
                    break;
                ++taskcnt;
                workerq.Push(j);
            }
            const std::size_t written=taskcnt ? R[taskcnt-1] : 0;
            // Parallel rounds have the same task count: each task covers
            // N/1024 original input entries. The next parallel round reads
            // exactly this written prefix, halved. Only the transition to
            // a full-domain serial round needs the otherwise untouched tail.
            const bool next_serial=i==9 || n-i-1<11;
            if (next_serial) folding.zeroOutputSuffix(written);
            logScalar(memory_stage,"worker_written_elements",written);
            logScalar(memory_stage,"initialized_padding_elements",
                      next_serial ? folding.outputLength()-written : 0);
            Fr Aa[40],Bb[40],Cc[40];
            memset(Aa,0,sizeof(Aa));
            memset(Bb,0,sizeof(Bb));
            memset(Cc,0,sizeof(Cc));
            // Joined (not detached) workers: a late-starting detached thread
            // could otherwise pop the next call's tasks with stale references.
            std::vector<std::thread> workers;
            zkgpt::ThreadJoiner joiner(workers);
            for(int j=0;j<thread_n;j++)
                workers.emplace_back(sc_last_worker,std::ref(Aa[j]),std::ref(Bb[j]),std::ref(Cc[j]),std::ref(read_1),std::ref(read_2),std::ref(write_1),std::ref(write_2),std::ref(L),std::ref(R),r_u[0][i-1]);
            joiner.join();
            endq.Clear();
            tt.stop();
            for(int j=0;j<thread_n;j++)
            {
                A+=Aa[j];
                B+=Bb[j];
                C+=Cc[j];
            }
        }
        else
        {
            Fr* read_1=i==1 ? p->lasso_mult_v.data() : folding.input1();
            Fr* read_2=i==1 ? comm.w : folding.input2();
            Fr* write_1=folding.output1(),*write_2=folding.output2();
            for(int kp=0;kp<(1<<(n-i));kp++)
            {
                zkgpt::lassoFoldEntry(read_1,read_2,write_1,write_2,kp,r_u[0][i-1]);
                if(kp&1)
                {
                    B+=write_1[kp]*write_2[kp];
                    C+=(write_1[kp]+write_1[kp]-write_1[kp-1])*(write_2[kp]+write_2[kp]-write_2[kp-1]);
                }
                else
                    A+=write_1[kp]*write_2[kp];
            }
        }
        
        if (i>0) {
            logResidentPages(memory_stage,"pb1",folding.output1(),folding.outputLength()*sizeof(Fr));
            logResidentPages(memory_stage,"pb2",folding.output2(),folding.outputLength()*sizeof(Fr));
        }
        logProcess(memory_stage+"_after_workers");
        if (i==2 && enabled()) {
            logCircuitStorage("lasso_fold_2_circuit_storage",this->C);
            p->logPrivateMemory("lasso_fold_2_private_storage");
            for (std::size_t layer_id=0;layer_id<p->val.size();++layer_id) {
                const auto &values=p->val[layer_id];
                const auto name="val_layer_"+std::to_string(layer_id);
                logVector("lasso_fold_2_private_storage",name,values);
                logResidentPages("lasso_fold_2_private_storage",name,values.data(),values.capacity()*sizeof(Fr));
            }
        }
        if (i>0) {
            folding.finishRound(); // Pointer changes/free occur only after join.
            logProcess(memory_stage+"_previous_released");
        }
        vtimer.start();
        Fr aa=(C+A)/2-B,bb=2*B-C/2-3*A/2,cc=A;
        Fr g1=aa+bb+cc,g0=cc;
        if(i!=n && previousSum!=g0+g1)
            throw std::runtime_error("Lasso sumcheck round claim failed");
        if(i!=n) {
            transcript_.appendFr("lasso.sumcheck.a", aa);
            transcript_.appendFr("lasso.sumcheck.b", bb);
            transcript_.appendFr("lasso.sumcheck.c", cc);
            gkr_proof_.lasso.rounds.push_back({aa,bb,cc});
            r_u[0][i]=challenge("lasso.sumcheck.challenge");
            previousSum=aa*r_u[0][i]*r_u[0][i]+bb*r_u[0][i]+cc;
        }
        vtimer.stop();
        builder_check_time+=vtimer.elapse_sec();
    }
    
    ptimer.stop();
    prover_time+=ptimer.elapse_sec();

    // 计算gr：为什么是证明者的时间？
    F gr = F_ZERO;

    eval_in=n==0 ? comm.w[0] : folding.input2()[0];
    transcript_.appendFr("lasso.input_evaluation", eval_in);
    gkr_proof_.lasso.input_evaluation=eval_in;
    folding.release();
    logProcess("lasso_scratch_released");
    delete[] L;
    delete[] R;

    beta_g.resize(1ULL << cur.bit_length);
    ptimer.start();
    
    initBetaTable(beta_g, cur.bit_length, r_0, F_ONE);
    for (u32 i = 1; i < C.size; ++i) 
    {
        const layer &lasso_layer = C.circuit.at(i);
        if (~lasso_layer.bit_length_u[0]) 
        {
            beta_u.resize(1ULL << lasso_layer.bit_length_u[0]);
            initBetaTable(beta_u, lasso_layer.bit_length_u[0],
                          r_u.at(i).begin(), sig_u.at(i - 1));
            for (u32 j = 0; j < lasso_layer.size_u[0]; ++j)
                gr += beta_g.at(lasso_layer.ori_id_u.at(j)) * beta_u.at(j);
        }

        if (~lasso_layer.bit_length_v[0]) 
        {
            beta_v.resize(1ULL << lasso_layer.bit_length_v[0]);
            initBetaTable(beta_v, lasso_layer.bit_length_v[0],
                          r_v.at(i).begin(), sig_v.at(i - 1));
            for (u32 j = 0; j < lasso_layer.size_v[0]; ++j)
                gr += beta_g.at(lasso_layer.ori_id_v.at(j)) * beta_v.at(j);
        }
        if (lasso_layer.ty==layerType::FCONN) {
            beta_u.resize(1ULL<<lasso_layer.bit_length_u[0]);
            initBetaTable(beta_u,lasso_layer.bit_length_u[0],
                          fconn_v_r_u.at(i).begin(),
                          sig_fconn_v_u.at(i-1));
            for (u32 j=0;j<lasso_layer.size_u[0];++j)
                gr+=beta_g.at(lasso_layer.ori_id_u.at(j))*beta_u.at(j);
            beta_v.resize(1ULL<<lasso_layer.bit_length_v[0]);
            initBetaTable(beta_v,lasso_layer.bit_length_v[0],
                          fconn_v_r_v.at(i).begin(),
                          sig_fconn_v_v.at(i-1));
            for (u32 j=0;j<lasso_layer.size_v[0];++j)
                gr+=beta_g.at(lasso_layer.ori_id_v.at(j))*beta_v.at(j);
        }
    }
    ptimer.stop();
    printf("GKR Lasso public input-mapping evaluation time: %f s\n",
           ptimer.elapse_sec());
    prover_time+=ptimer.elapse_sec();
    
    beta_u.clear();
    beta_v.clear();

    vtimer.start();
    if (eval_in*gr!=previousSum)
        throw std::runtime_error("Lasso final input evaluation check failed");
    transcript_.appendFr("lasso.mapping_evaluation", gr);
    gkr_proof_.lasso.mapping_evaluation=gr;
    vtimer.stop();
    builder_check_time+=vtimer.elapse_sec();

    fprintf(stderr, "GKR proof-builder consistency checks passed\n");

    beta_g.clear();
    beta_gs.clear();
    beta_u.clear();
    beta_v.clear();
    r_u.resize(1);
    r_v.clear();

    sig_u.clear();
    sig_v.clear();
    return true;
}
__int128 convertx(Fr x)	
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
    uint8_t bf[16]={0};	 //64 bit
    int size=abs.getLittleEndian(bf,16);	
    __int128 V=0;	
    for(int j=size-1;j>=0;j--)
    {	
        V=(V<<8)+bf[j];	
    }
    if(sign)	
        V=-V;	
    return V;	
}

bool GKRProofBuilder::buildInputOpeningProof()
{
    if (comm.l<0 || comm.l>30 || r_u.empty() ||
        r_u[0].size()!=static_cast<std::size_t>(comm.l))
        throw std::runtime_error("input commitment opening shape mismatch");
    if (!comm.w || !comm.comm || !comm.g)
        throw std::runtime_error("null input commitment opening storage");
    const std::size_t value_count=static_cast<std::size_t>(1)<<comm.l;
    const std::size_t row_count=static_cast<std::size_t>(1)<<(comm.l/2);
    const std::size_t generator_count=static_cast<std::size_t>(1)
        <<(comm.l-comm.l/2);
    if (public_statement_.val0_generator_domain.count!=generator_count)
        throw std::runtime_error("input commitment generator count mismatch");
    const auto &generator_set=getGeneratorSet(
        public_statement_.val0_generator_domain);
    for (std::size_t i=0;i<generator_count;++i)
        if (comm.g[i]!=generator_set.generators[i])
            throw std::runtime_error("input commitment generator mismatch");
    if (comm.G!=generator_set.u)
        throw std::runtime_error("input commitment U generator mismatch");

    vector<G1> commitments(comm.comm, comm.comm+row_count);
    timer prover_timer;
    zkgpt::memory_diagnostics::logProcess("input_opening_begin");
    const HyraxFrWitnessView witness(comm.w,value_count);
    zkgpt::memory_diagnostics::logProcess("input_opening_view_ready");
    zkgpt::memory_diagnostics::logScalar("input_opening_view_ready","borrowed_witness_elements",value_count);
    zkgpt::memory_diagnostics::logScalar("input_opening_view_ready","full_witness_copy_allocation_bytes",0);
    zkgpt::memory_diagnostics::logScalar("input_opening_view_ready","full_witness_copy_bytes",0);
    zkgpt::memory_diagnostics::logScalar("input_opening_view_ready","padded_witness_buffers",1);
    // Keep the historical arithmetic-only Hyrax timing boundary.
    prover_timer.start();
    const auto opening=hyraxMleOpenProveFrRowMajor(
        witness, commitments, r_u[0], generator_set.generators,
        generator_set.u, eval_in, transcript_, "gkr.input.opening");
    prover_timer.stop();
    zkgpt::memory_diagnostics::logProcess("input_opening_complete");
    gkr_proof_.lasso.input_opening=opening;
    prover_time+=prover_timer.elapse_sec();

    printf("GKR input-commitment MLE opening time: proof generation %f s\n",
           prover_timer.elapse_sec());
    return true;
}
