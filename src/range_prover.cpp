#include "range_prover.hpp"
#include "hyrax_opening.hpp"
#include "sparse_opening.hpp"
#include "range_logup.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utils.hpp>

static vector<F> beta_gs, beta_u;
using namespace mcl::bn;
using std::unique_ptr;

int lg2(int x) 
{
    int res = 0;
    while (x >>= 1) ++res;
    return res;
}
Fr poly_eval(Fr x0,Fr x1,Fr x2,Fr x3,Fr u)  //lagrange poly for degree 3
{
    Fr y=1/Fr(6)*((-x0)*(u-1)*(u-2)*(u-3)+3*x1*u*(u-2)*(u-3)-3*x2*u*(u-1)*(u-3)+x3*u*(u-1)*(u-2));
    return y;
}

range_prover::SC_Return range_prover::sumcheck_deg1(int l, Fr* f, Fr S) // sum_i f_i=S
{
    if (l < 0 || l > MAXL)
        throw std::invalid_argument("invalid degree-1 sumcheck length");
    // P send V sum S
    Fr *ran=new Fr[l];
    bool owns_f=false;
    auto fail = [&](const char *message) {
        delete[] ran;
        if (owns_f) delete[] f;
        throw std::runtime_error(message);
    };
    for(int i=l;i>=1;i--) // round i
    {
        Fr sum0=0,sum1=0;
        for(int j=0;j<(1<<i);j++)
        {
            if((j&1)==0)
                sum0+=f[j];
            else
            {
                sum1+=f[j];
            }
        }
        if (sum0 + sum1 != S)
            fail("range degree-1 sumcheck round consistency failed");
        //send poly: sum0,sum1
        Fr new_chlg;
        new_chlg.setByCSPRNG();
        ran[l-i]=new_chlg;
        //V send new_chlg
        S=new_chlg*(sum1-sum0)+sum0; // update target sum
        Fr* new_f=new Fr[1<<(i-1)];
        Fr sum0p=0,sum1p=0;
        for(int j=0;j<(1<<(i-1));j++)
            new_f[j]=(1-new_chlg)*f[j*2]+new_chlg*f[j*2+1];
        if (owns_f) delete[] f;
        f=new_f;
        owns_f=true;
    }
    if (f[0] != S)
        fail("range degree-1 sumcheck final claim failed");
    SC_Return s;
    s.random=ran;
    s.claim_f=f[0];
    if (owns_f) delete[] f;
    return s;
}

range_prover::SC_Return range_prover::sumcheck_deg3(int l, Fr* r, Fr* f, Fr* g, Fr S) // sum_i eq(r,i) f_i g_i=S
{
    if (l < 0 || l > MAXL)
        throw std::invalid_argument("invalid degree-3 sumcheck length");
    Fr* lag=range_proof_get_eq(r,l);
    // P send V sum S
    Fr *ran=new Fr[l];
    Fr *S0=new Fr[1<<l],*S1=new Fr[1<<l],*S2=new Fr[1<<l],*S3=new Fr[1<<l];
    bool owns_f=false;
    bool owns_g=false;
    auto fail = [&](const char *message) {
        delete[] ran;
        if (owns_f) delete[] f;
        if (owns_g) delete[] g;
        delete[] lag;
        delete[] S0;
        delete[] S1;
        delete[] S2;
        delete[] S3;
        throw std::runtime_error(message);
    };
    for(int i=l;i>=1;i--) // round i
    {
        memset(S0,0,sizeof(Fr)*(1<<i));
        memset(S1,0,sizeof(Fr)*(1<<i));
        memset(S2,0,sizeof(Fr)*(1<<i));
        memset(S3,0,sizeof(Fr)*(1<<i));

        Fr sum0=0,sum1=0,sum2=0,sum3=0;

        #pragma omp parallel for
        for(int j=0;j<(1<<i);j+=2)
        {
            S0[j>>1]=lag[j]*f[j]*g[j];
            S1[j>>1]=lag[j+1]*f[j+1]*g[j+1];
            S2[j>>1]=(lag[j+1]+lag[j+1]-lag[j])*(f[j+1]+f[j+1]-f[j])*(g[j+1]+g[j+1]-g[j]);
            S3[j>>1]=(lag[j+1]+lag[j+1]+lag[j+1]-lag[j]-lag[j])*(f[j+1]+f[j+1]+f[j+1]-f[j]-f[j])*(g[j+1]+g[j+1]+g[j+1]-g[j]-g[j]);  
        }

        if(i<8)
        {
            for(int j=0;j<(1<<(i-1));j++)
            {
                sum0+=S0[j];
                sum1+=S1[j];
                sum2+=S2[j];
                sum3+=S3[j];
            }
        }
        else
        {
            Fr s0[8],s1[8],s2[8],s3[8];
            memset(s0,0,sizeof(Fr)*8);
            memset(s1,0,sizeof(Fr)*8);
            memset(s2,0,sizeof(Fr)*8);
            memset(s3,0,sizeof(Fr)*8);
            #pragma omp parallel for
            for(int k=0;k<(1<<3);k++)
            {
                for(int j=0;j<(1<<(i-1-3));j++)
                {
                    s0[k]+=S0[(k<<(i-1-3))+j];
                    s1[k]+=S1[(k<<(i-1-3))+j];
                    s2[k]+=S2[(k<<(i-1-3))+j];
                    s3[k]+=S3[(k<<(i-1-3))+j];
                }
            }
            sum0=s0[0]+s0[1]+s0[2]+s0[3]+s0[4]+s0[5]+s0[6]+s0[7];
            sum1=s1[0]+s1[1]+s1[2]+s1[3]+s1[4]+s1[5]+s1[6]+s1[7];
            sum2=s2[0]+s2[1]+s2[2]+s2[3]+s2[4]+s2[5]+s2[6]+s2[7];
            sum3=s3[0]+s3[1]+s3[2]+s3[3]+s3[4]+s3[5]+s3[6]+s3[7];
        }
        if (sum0 + sum1 != S)
            fail("range degree-3 sumcheck round consistency failed");
        //send poly: sum0,sum1,sum2,sum3
        Fr new_chlg;
        new_chlg.setByCSPRNG();
        ran[l-i]=new_chlg;
        //V send new_chlg
        S=poly_eval(sum0,sum1,sum2,sum3,new_chlg); // update target sum
        Fr* new_lag=new Fr[1<<(i-1)];
        Fr* new_f=new Fr[1<<(i-1)];
        Fr* new_g=new Fr[1<<(i-1)];
        Fr sum0p=0,sum1p=0;
        #pragma omp parallel for 
        for(int j=0;j<(1<<(i-1));j++)
        {
            new_lag[j]=lag[j*2]+new_chlg*(lag[j*2+1]-lag[j*2]);
            new_f[j]=f[j*2]+new_chlg*(f[j*2+1]-f[j*2]);
            new_g[j]=g[j*2]+new_chlg*(g[j*2+1]-g[j*2]);
        }
        if (owns_f) delete[] f;
        if (owns_g) delete[] g;
        delete[] lag;
        f=new_f;
        g=new_g;
        lag=new_lag;
        owns_f=true;
        owns_g=true;
    }
    if (lag[0] * f[0] * g[0] != S)
        fail("range degree-3 sumcheck final claim failed");
    SC_Return s;
    s.random=ran;
    s.claim_f=f[0];
    s.claim_g=g[0];
    if (owns_f) delete[] f;
    if (owns_g) delete[] g;
    delete[] lag;
    delete[] S0;
    delete[] S1;
    delete[] S2;
    delete[] S3;
    return s;
}
void range_prover::init() {
    // 初始化一组生成元g
    GG=range_proof_gen_gi(g,1<<(MAXL-MAXL/2));
}
void range_prover::push_back(NonlinearOpType op_type, const std::vector<std::pair<int, int>>& constraint_params) {
    OP op;
    op.op_type = op_type;
    for (const auto &parameter : constraint_params) {
        const int query_size=parameter.first;
        const int range_size=parameter.second;
        Constraint c;
        c.query_size = query_size;
        c.range_size = range_size;
        c.actual_query_size = query_size;
        c.inputs.assign(query_size, range_wide::EncodedWitnessValue{});
        buildChunkQueries(c);
        op.constraints.push_back(c);
    }
    ops.push_back(op);
}

timer range_proof_commit_timer;
double range_proof_commit_time=0;
timer range_proof_open_timer;
double range_proof_open_time=0;
timer range_proof_sumcheck_timer;
double range_proof_sumcheck_time=0;
timer temp_timer;
double temp_time=0;
// range_prove(constraint.inputs, constraint.range_size, constraint.query_size, 32);
vector<vector<G1>> range_prover::range_prove(
    const Constraint &constraint, int thread_num) {
    const int m=constraint.query_size;
    vector<vector<G1>> commitments;
    commitments.reserve(constraint.chunk_queries.size());
    for(std::size_t i=0;i<constraint.chunk_queries.size();++i)
    {
        const auto &query=constraint.chunk_queries[i];
        const int chunk_bits=static_cast<int>(query.chunk_bits);
        const int table_size=1<<chunk_bits;
        std::cout << "  proving chunk " << i << " (" << chunk_bits
                  << " bits)" << std::endl;
        vector<ll> chunk_values(query.chunks.begin(), query.chunks.end());
        vector<ll> table(table_size);
        for(int j=0;j<table_size;j++) table[j]=j;
        commitments.push_back(logup(chunk_values.data(),table.data(),m,
                                    table_size,thread_num));
    }
    printf("range proof time: commit time %.6f s, open time %.6f s,sumcheck_time %.6f s\n",range_proof_commit_time,range_proof_open_time,range_proof_sumcheck_time);
    printf("temp time: %.6f s\n",temp_time);
    return commitments;
}
// 证明分片f[m]的所有元素都属于集合t[n]
vector<G1> range_prover::logup(ll * f,ll *t,int m,int n,int thread)
{
    Fr* G=new Fr[m];  // 存储1/(f[i]+r)
    Fr* F=new Fr[m];  // 存储f[i]+r（隐藏原始值）
    Fr* Hp=new Fr[n]; // 存储t[i]+r
    Fr* H=new Fr[n];  // 存储c[i]/(t[i]+r)
    ll * c=new ll[n]; // 统计t中元素在f中出现的次数
    memset(c,0,sizeof(ll)*n);
    for(int i=0;i<m;i++)
        c[f[i]]++;
    
    Fr r;
    r.setByCSPRNG();
    temp_timer.start();
    for(int i=0;i<m;i++)
        F[i]=r+Fr(f[i]);
    for(int i=0;i<n;i++)
        Hp[i]=r+Fr(t[i]);
    
    invVec(G,F,m);  // 1/(r+fi)
    invVec(H,Hp,n);  // 1/(r+ti)
    for(int i=0;i<n;i++)
        H[i]=H[i]*Fr(c[i]);
    temp_timer.stop();
    temp_time+=temp_timer.elapse_sec();

    range_proof_commit_timer.start();
    G1* f_comm=range_proof_prover_commit(f,g,lg2(m),thread);
    const int f_commitment_count=1<<(lg2(m)/2);
    vector<G1> f_commitments(f_comm, f_comm+f_commitment_count);
    G1* t_comm=range_proof_prover_commit(t,g,lg2(n),thread);
    G1* c_comm=range_proof_prover_commit(c,g,lg2(n),thread);
    Fr *diff=new Fr[n];
    for(int i=0;i<n;i++)
    {
        diff[i]=1/(r+t[i]);
    }

    G1* g_comm=range_proof_prover_commit_fr(f,diff,n,g,lg2(m),thread);    // 对承诺查询表项的倒数进行优化,合并查找
    G1* h_comm=range_proof_prover_commit_fr_general(H,g,lg2(n),thread);
    //printf("range proof commitments_size:%d\n",f_comm->getSerializedByteSize()*lg2(m)+t_comm->getSerializedByteSize()*lg2(n)+c_comm->getSerializedByteSize()*lg2(n)+g_comm->getSerializedByteSize()*lg2(m)+h_comm->getSerializedByteSize()*lg2(n));


    Fr* rp1=new Fr[lg2(n)];
    for(int i=0;i<lg2(n);i++)
        rp1[i].setByCSPRNG(); //verifier challenge

    Fr c_eva=range_proof_prover_evaluate(c,rp1,lg2(n));
    range_proof_commit_timer.stop();
    range_proof_commit_time+=range_proof_commit_timer.elapse_sec();
    
    range_proof_sumcheck_timer.start();
    range_prover::SC_Return ret1=sumcheck_deg3(lg2(n),rp1,H,Hp,c_eva); // sumcheck, reciprocal well-formed   Hi*(r+ti)=ci

    Fr* rp2=new Fr[lg2(m)];
    for(int i=0;i<lg2(m);i++)
        rp2[i].setByCSPRNG(); //verifier challenge
    range_prover::SC_Return ret2=sumcheck_deg3(lg2(m),rp2,G,F,1); // sumcheck, reciprocal well-formed  Gi*(r+fi)=1
    Fr sum=0;
    for(int i=0;i<m;i++)
        sum+=G[i];
    range_prover::SC_Return ret3=sumcheck_deg1(lg2(m),G,sum); // sumcheck, reciprocal sum
    
    range_prover::SC_Return ret4=sumcheck_deg1(lg2(n),H,sum); // sumcheck, reciprocal sum
    range_proof_sumcheck_timer.stop();
    range_proof_sumcheck_time+=range_proof_sumcheck_timer.elapse_sec();

    range_proof_open_timer.start();
    range_proof_open(c,rp1,c_eva,GG,g,c_comm,lg2(n));
    range_proof_open(H,ret1.random,ret1.claim_f,GG,g,h_comm,lg2(n));
    range_proof_open(t,ret1.random,ret1.claim_g-r,GG,g,t_comm,lg2(n));
    range_proof_open(G,ret2.random,ret2.claim_f,GG,g,g_comm,lg2(m));
    range_proof_open(f,ret2.random,ret2.claim_g-r,GG,g,f_comm,lg2(m));
    range_proof_open(G,ret3.random,ret3.claim_f,GG,g,g_comm,lg2(m));
    range_proof_open(H,ret4.random,ret4.claim_f,GG,g,h_comm,lg2(n));
    range_proof_open_timer.stop();
    range_proof_open_time+=range_proof_open_timer.elapse_sec();
    delete[] ret1.random;
    delete[] ret2.random;
    delete[] ret3.random;
    delete[] ret4.random;
    delete[] G;
    delete[] F;
    delete[] Hp;
    delete[] H;
    delete[] c;
    delete[] diff;
    delete[] f_comm;
    delete[] t_comm;
    delete[] c_comm;
    delete[] g_comm;
    delete[] h_comm;
    return f_commitments;
}

double range_prover::prove() {
    return proveMembership(nullptr);
}

double range_prover::proveMembership(
    vector<vector<vector<G1>>> *chunk_commitments) {
    if (!built_from_witness)
        throw std::logic_error("Range Proof must be built from the registered witness");
    verifyWitnessConsistency();
    if (chunk_commitments) chunk_commitments->clear();
    double prover_time = 0;
    for (auto& op : ops) {
        for (auto& constraint : op.constraints) {
            cout << "start range prove"<<" query_size "<< constraint.query_size<<"\t";
            cout << " range_size "<< constraint.range_size << endl;
            prove_timer.start();
            auto commitments=range_prove(constraint, thread_num);
            if (chunk_commitments)
                chunk_commitments->push_back(std::move(commitments));
            cout << "end range prove" << endl;
            prove_timer.stop();
            prover_time += prove_timer.elapse_sec();
            cout << "time: " << prove_timer.elapse_sec() << endl;
        }
    }
    return prover_time;
}

RangePublicStatement range_prover::makePublicStatement(
    int val0_log_size, const G1 *val0_commitments,
    std::size_t commitment_count, const G1 *val0_generators,
    std::size_t generator_count, const G1 &val0_u) const {
    if (!built_from_witness || val0_commitments==nullptr ||
        val0_generators==nullptr)
        throw std::logic_error("cannot make a public statement before buildFromWitness");
    if (val0_log_size<0 || val0_log_size>62)
        throw std::invalid_argument("invalid val[0] commitment log size");
    const std::size_t expected=static_cast<std::size_t>(1)
                               << (val0_log_size/2);
    if (commitment_count!=expected)
        throw std::invalid_argument("val[0] commitment row count mismatch");
    const std::size_t expected_generators=static_cast<std::size_t>(1)
        << (val0_log_size-val0_log_size/2);
    if (generator_count!=expected_generators)
        throw std::invalid_argument("val[0] generator count mismatch");
    const std::size_t val0_capacity=static_cast<std::size_t>(1)<<val0_log_size;
    if (witness_source==nullptr || witness_source->size()>val0_capacity)
        throw std::invalid_argument("val[0] witness exceeds commitment capacity");

    RangePublicStatement statement;
    statement.val0_log_size=val0_log_size;
    statement.val0_commitment.assign(val0_commitments,
                                     val0_commitments+commitment_count);
    statement.val0_generators.assign(val0_generators,
                                     val0_generators+generator_count);
    statement.val0_u=val0_u;
    statement.range_generators.assign(g, g+(1<<(MAXL-MAXL/2)));
    statement.range_u=GG;
    statement.shape=witness_shape;
    statement.regions.reserve(query_regions.size());
    for (const auto &region : query_regions)
        statement.regions.push_back({region.kind, region.name,
            region.val0_offset, region.count, region.bits, region.is_signed,
            region.proof_constraint_index, region.proof_start});
    const auto &constraints=ops.front().constraints;
    statement.queries.reserve(constraints.size());
    for (const auto &constraint : constraints) {
        PublicRangeQuery query;
        query.bits=constraint.range_size;
        query.actual_query_size=constraint.actual_query_size;
        query.padded_query_size=constraint.query_size;
        for (const auto &chunk : constraint.chunk_queries)
            query.chunk_bits.push_back(chunk.chunk_bits);
        statement.queries.push_back(std::move(query));
    }
    return statement;
}

namespace {

Fr evaluateMultilinear(vector<Fr> values, const vector<Fr> &challenges) {
    std::size_t active=values.size();
    for (const Fr &challenge : challenges) {
        if (active<2 || (active&1)!=0)
            throw std::logic_error("invalid multilinear evaluation shape");
        for (std::size_t i=0;i<active/2;++i)
            values[i]=values[2*i]+challenge*(values[2*i+1]-values[2*i]);
        active/=2;
    }
    if (active!=1)
        throw std::logic_error("multilinear evaluation challenge count mismatch");
    return values[0];
}

}  // namespace

ReconstructionProof range_prover::proveReconstruction(
    std::size_t query_index, const Constraint &constraint,
    Transcript &transcript, vector<Fr> *point) const {
    ReconstructionProof proof;
    proof.query_index=query_index;
    proof.initial_claim=0;

    const std::size_t query_size=static_cast<std::size_t>(constraint.query_size);
    unsigned rounds=0;
    for (std::size_t n=query_size;n>1;n>>=1) ++rounds;
    for (unsigned round=0;round<rounds;++round) {
        ReconstructionRound message;
        message.sum0=0;
        message.sum1=0;
        proof.rounds.push_back(message);
    }
    vector<Fr> challenges=deriveReconstructionPoint(transcript, proof);
    if (point) *point=challenges;
    proof.final_claim=0;

    vector<Fr> encoded(query_size);
    for (std::size_t i=0;i<query_size;++i) {
        std::vector<std::uint16_t> chunks;
        chunks.reserve(constraint.chunk_queries.size());
        for (const auto &chunk : constraint.chunk_queries)
            chunks.push_back(chunk.chunks[i]);
        const auto reconstructed=range_wide::reconstructWide(
            chunks, constraint.range_size);
        if (reconstructed!=constraint.inputs[i].encoded)
            throw std::logic_error("cannot prove an invalid chunk reconstruction");
        encoded[i].setStr(range_wide::unsignedWideToString(
            constraint.inputs[i].encoded));
    }
    proof.encoded_evaluation=evaluateMultilinear(std::move(encoded), challenges);

    proof.chunk_evaluations.reserve(constraint.chunk_queries.size());
    for (const auto &chunk : constraint.chunk_queries) {
        vector<Fr> values(query_size);
        for (std::size_t i=0;i<query_size;++i) values[i]=Fr(chunk.chunks[i]);
        proof.chunk_evaluations.push_back(
            evaluateMultilinear(std::move(values), challenges));
    }

    Fr reconstructed_evaluation=0;
    Fr weight=1;
    for (std::size_t i=0;i<proof.chunk_evaluations.size();++i) {
        reconstructed_evaluation+=proof.chunk_evaluations[i]*weight;
        for (unsigned bit=0;bit<constraint.chunk_queries[i].chunk_bits;++bit)
            weight+=weight;
    }
    if (proof.encoded_evaluation-reconstructed_evaluation!=proof.final_claim)
        throw std::logic_error("batched reconstruction evaluation failed");

    transcript.appendFr("reconstruction.final_claim", proof.final_claim);
    transcript.appendFr("reconstruction.encoded_evaluation",
                        proof.encoded_evaluation);
    for (const auto &evaluation : proof.chunk_evaluations)
        transcript.appendFr("reconstruction.chunk_evaluation", evaluation);
    return proof;
}

RangeProof range_prover::proveStageB(const RangePublicStatement &statement) {
    RangeProof proof;
    if (!built_from_witness || ops.empty())
        throw std::logic_error("Stage B Range Proof is not initialized");
    verifyWitnessConsistency();
    timer membership_timer;
    membership_timer.start();
    const auto &constraints=ops.front().constraints;
    proof.membership_proofs.resize(constraints.size());
    proof.chunk_commitments.resize(constraints.size());
    for (std::size_t query_index=0;query_index<constraints.size();++query_index) {
        const auto &constraint=constraints[query_index];
        auto &query_proofs=proof.membership_proofs[query_index];
        auto &query_commitments=proof.chunk_commitments[query_index];
        query_proofs.reserve(constraint.chunk_queries.size());
        query_commitments.reserve(constraint.chunk_queries.size());
        std::cout << "start Stage B membership query_size "
                  << constraint.query_size << "\t range_size "
                  << constraint.range_size << std::endl;
        for (std::size_t chunk_index=0;
             chunk_index<constraint.chunk_queries.size();++chunk_index) {
            const auto &chunk=constraint.chunk_queries[chunk_index];
            std::cout << "  proving independent LogUp chunk " << chunk_index
                      << " (" << chunk.chunk_bits << " bits)" << std::endl;
            query_proofs.push_back(proveLogUp(
                query_index, chunk_index, chunk.chunk_bits, chunk.chunks,
                statement.range_generators, statement.range_u, thread_num));
            query_commitments.push_back(
                query_proofs.back().value_commitment);
        }
    }
    membership_timer.stop();
    proof.membership_prover_time=membership_timer.elapse_sec();
    timer reconstruction_timer;
    reconstruction_timer.start();
    Transcript transcript("zkGPT-range-stage-b-v1");
    appendRangeStatement(transcript, statement);
    appendChunkCommitments(transcript, proof);
    proof.reconstruction_proofs.reserve(constraints.size());
    proof.chunk_openings.resize(constraints.size());
    proof.val0_openings.reserve(constraints.size());
    for (std::size_t i=0;i<constraints.size();++i) {
        vector<Fr> point;
        proof.reconstruction_proofs.push_back(
            proveReconstruction(i, constraints[i], transcript, &point));
        auto &openings=proof.chunk_openings[i];
        openings.reserve(constraints[i].chunk_queries.size());
        for (std::size_t chunk_index=0;
             chunk_index<constraints[i].chunk_queries.size();++chunk_index) {
            ChunkOpeningProof opening;
            opening.query_index=i;
            opening.chunk_index=chunk_index;
            opening.claimed_evaluation=
                proof.reconstruction_proofs.back().chunk_evaluations[chunk_index];
            const std::string label="chunk-mle/"+std::to_string(i)+"/"+
                                    std::to_string(chunk_index);
            opening.opening=hyraxMleOpenProve(
                constraints[i].chunk_queries[chunk_index].chunks,
                proof.chunk_commitments[i][chunk_index], point,
                statement.range_generators, statement.range_u,
                opening.claimed_evaluation, transcript, label);
            openings.push_back(std::move(opening));
        }
        if (witness_source==nullptr)
            throw std::logic_error("val[0] witness is unavailable for opening");
        proof.val0_openings.push_back(proveSparseVal0Opening(
            statement, i, point, *witness_source,
            proof.reconstruction_proofs.back().encoded_evaluation,
            transcript));
    }
    proof.transcript_binding=transcript.challenge("range-proof-final");
    reconstruction_timer.stop();
    proof.reconstruction_prover_time=reconstruction_timer.elapse_sec();
    return proof;
}

int next_power_of_2(int n) {
    int power = 1;
    while (power < n) {
        power *= 2;
    }
    return power;
}

void range_prover::build() {
    throw std::logic_error(
        "range_prover::build() generated unbound random inputs and is disabled; "
        "use buildFromWitness()");
}

void range_prover::validateShape(const WitnessShape &shape) {
    auto check_or_set = [](int &configured, std::size_t actual,
                           const char *name) {
        if (actual > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::overflow_error(std::string(name) + " exceeds int range");
        if (configured != 0 && configured != static_cast<int>(actual)) {
            std::ostringstream message;
            message << "Range Proof " << name << " mismatch: configured="
                    << configured << ", witness=" << actual;
            throw std::invalid_argument(message.str());
        }
        configured = static_cast<int>(actual);
    };
    check_or_set(seq_len, shape.sequence_length, "sequence length");
    check_or_set(LayerNum, shape.layer_count, "layer count");
    check_or_set(HeadNum, shape.head_count, "head count");
    check_or_set(HeadDim, shape.head_dimension, "head dimension");
    check_or_set(AttnDim, shape.hidden_dimension, "hidden dimension");
    check_or_set(LinearDim, shape.mlp_dimension, "MLP dimension");
    if (HeadNum * HeadDim != AttnDim)
        throw std::invalid_argument("Range Proof attention dimensions are inconsistent");
}

range_wide::EncodedWitnessValue range_prover::encodeWitnessValue(
    const F &field_value, const RangeConstraint &constraint,
    std::size_t absolute_offset) {
    const bool negative=field_value.isNegative();
    F magnitude_field=negative ? -field_value : field_value;
    std::array<std::uint8_t, 32> bytes{};
    const int byte_count=magnitude_field.getLittleEndian(
        bytes.data(), bytes.size());
    range_wide::UnsignedWide magnitude=0;
    bool magnitude_too_large=byte_count<0 || byte_count>16;
    if (!magnitude_too_large) {
        for (int i=byte_count-1;i>=0;--i)
            magnitude=magnitude*256+bytes[static_cast<std::size_t>(i)];
        magnitude_too_large=
            magnitude>=range_wide::limitForBits(range_wide::kMaxBits);
    }
    try {
        if (magnitude_too_large)
            throw std::range_error("field magnitude exceeds Range Proof capacity");
        const range_wide::SignedWide value=negative
            ? -static_cast<range_wide::SignedWide>(magnitude)
            : static_cast<range_wide::SignedWide>(magnitude);
        const auto encoded=constraint.is_signed
            ? range_wide::encodeSigned(value, constraint.bits)
            : range_wide::encodeUnsigned(value, constraint.bits);
        return {encoded};
    } catch (const std::exception &) {
        std::ostringstream message;
        message << "Range Proof value out of bounds for " << constraint.name
                << " at val[0][" << absolute_offset << "]: magnitude=";
        if (magnitude_too_large)
            message << ">=2^" << range_wide::kMaxBits;
        else
            message << (negative ? "-" : "")
                    << range_wide::unsignedWideToString(magnitude);
        message << ", bits=" << constraint.bits
                << ", signed=" << (constraint.is_signed ? "true" : "false");
        throw std::range_error(message.str());
    }
}

void range_prover::buildChunkQueries(Constraint &constraint) {
    const unsigned chunk_count=(constraint.range_size+
        range_wide::kDefaultChunkBits-1)/range_wide::kDefaultChunkBits;
    constraint.chunk_queries.assign(chunk_count, {});
    for (unsigned chunk_index=0;chunk_index<chunk_count;++chunk_index) {
        auto &query=constraint.chunk_queries[chunk_index];
        const unsigned offset=chunk_index*range_wide::kDefaultChunkBits;
        query.chunk_bits=std::min(range_wide::kDefaultChunkBits,
                                  constraint.range_size-offset);
        query.chunks.reserve(constraint.inputs.size());
    }
    for (const auto &input : constraint.inputs) {
        const auto chunks=range_wide::decomposeWide(
            input.encoded, constraint.range_size);
        if (range_wide::reconstructWide(chunks, constraint.range_size)!=
            input.encoded)
            throw std::logic_error("Range Proof wide-value reconstruction failed");
        for (unsigned i=0;i<chunk_count;++i)
            constraint.chunk_queries[i].chunks.push_back(chunks[i]);
    }
}

void range_prover::buildFromWitness(const vector<F> &val0,
                                    const WitnessRegistry &registry) {
    registry.validateLayout(val0.size());
    validateShape(registry.shape());
    witness_shape=registry.shape();
    ops.clear();
    query_regions.clear();
    witness_source=&val0;
    built_from_witness=false;

    OP op;
    op.op_type=NonlinearOpType::NonLinear;
    // The legacy sumcheck materializes several Fr work arrays per query.
    // Capping at 2^22 keeps peak memory bounded while MAXL remains the
    // protocol-wide hard limit.
    constexpr std::size_t max_query_size=static_cast<std::size_t>(1)<<22;
    std::map<unsigned, std::size_t> current_group;

    for (const auto &constraint : registry.constraints()) {
        if (constraint.count > max_query_size)
            throw std::length_error("single range constraint exceeds Range Proof capacity: " +
                                    constraint.name);
        auto group_it=current_group.find(constraint.bits);
        if (group_it==current_group.end() ||
            op.constraints[group_it->second].inputs.size()+constraint.count >
                max_query_size) {
            Constraint group;
            group.range_size=constraint.bits;
            op.constraints.push_back(std::move(group));
            current_group[constraint.bits]=op.constraints.size()-1;
            group_it=current_group.find(constraint.bits);
        }

        Constraint &group=op.constraints[group_it->second];
        const std::size_t proof_start=group.inputs.size();
        group.inputs.reserve(proof_start+constraint.count);
        for (std::size_t i=0;i<constraint.count;++i) {
            group.inputs.push_back(encodeWitnessValue(
                val0[constraint.val0_offset+i], constraint,
                constraint.val0_offset+i));
        }
        query_regions.push_back({constraint.kind, constraint.name,
                                 constraint.val0_offset, constraint.count,
                                 constraint.bits, constraint.is_signed,
                                 group_it->second, proof_start});
    }

    for (auto &constraint : op.constraints) {
        constraint.actual_query_size=constraint.inputs.size();
        std::size_t padded=1;
        while (padded<constraint.actual_query_size) padded<<=1;
        if (padded>max_query_size)
            throw std::length_error("Range Proof query exceeds 2^MAXL");
        constraint.inputs.resize(padded, range_wide::EncodedWitnessValue{});
        constraint.query_size=static_cast<int>(padded);
        buildChunkQueries(constraint);
    }
    if (op.constraints.empty())
        throw std::invalid_argument("witness registry contains no range constraints");
    ops.push_back(std::move(op));
    built_from_witness=true;

    std::cout << "Range Proof built directly from val[0]: "
              << query_regions.size() << " registered regions across "
              << ops.front().constraints.size() << " range queries" << std::endl;
    for (std::size_t i=0;i<ops.front().constraints.size();++i) {
        const auto &constraint=ops.front().constraints[i];
        std::cout << "  query " << i << ": bits=" << constraint.range_size
                  << ", values=" << constraint.actual_query_size
                  << ", padded=" << constraint.query_size << std::endl;
    }
}

void range_prover::verifyWitnessConsistency() const {
    if (!built_from_witness || witness_source==nullptr || ops.empty())
        throw std::logic_error("Range Proof witness mapping is not initialized");
    for (const auto &region : query_regions) {
        if (region.val0_offset>witness_source->size() ||
            region.count>witness_source->size()-region.val0_offset)
            throw std::out_of_range("Range Proof source witness changed size");
        const RangeConstraint constraint{region.kind, region.name,
                                         region.val0_offset, region.count,
                                         region.bits, region.is_signed};
        const Constraint &proof_constraint=
            ops.front().constraints.at(region.proof_constraint_index);
        for (std::size_t i=0;i<region.count;++i) {
            const auto expected=encodeWitnessValue(
                witness_source->at(region.val0_offset+i), constraint,
                region.val0_offset+i);
            if (proof_constraint.inputs.at(region.proof_start+i)!=expected) {
                std::ostringstream message;
                message << "Range Proof copy no longer matches val[0] for "
                        << region.name << " at relative index " << i;
                throw std::logic_error(message.str());
            }
            std::vector<std::uint16_t> chunks;
            chunks.reserve(proof_constraint.chunk_queries.size());
            for (const auto &query : proof_constraint.chunk_queries)
                chunks.push_back(query.chunks.at(region.proof_start+i));
            if (range_wide::reconstructWide(chunks, region.bits)!=
                expected.encoded)
                throw std::logic_error("Range Proof chunks no longer reconstruct val[0]");
        }
    }
}

void range_prover::tamperBuiltValueForTest(std::size_t region_index) {
    if (!built_from_witness)
        throw std::logic_error("cannot tamper an uninitialized Range Proof");
    const auto &region=query_regions.at(region_index);
    Constraint &constraint=ops.front().constraints.at(region.proof_constraint_index);
    constraint.inputs.at(region.proof_start).encoded+=1;
}
