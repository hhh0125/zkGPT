#include "range_prover.hpp"
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
    // P send V sum S
    Fr *ran=new Fr[l];
    bool owns_f=false;
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
        // TODO(protocol): the verifier must check sum0 + sum1 == S.
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
    SC_Return s;
    s.random=ran;
    s.claim_f=f[0];
    if (owns_f) delete[] f;
    return s;
}

range_prover::SC_Return range_prover::sumcheck_deg3(int l, Fr* r, Fr* f, Fr* g, Fr S) // sum_i eq(r,i) f_i g_i=S
{
    Fr* lag=range_proof_get_eq(r,l);
    // P send V sum S
    Fr *ran=new Fr[l];
    Fr *S0=new Fr[1<<l],*S1=new Fr[1<<l],*S2=new Fr[1<<l],*S3=new Fr[1<<l];
    bool owns_f=false;
    bool owns_g=false;
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
        // TODO(protocol): the verifier must check sum0 + sum1 == S.
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
        c.inputs.assign(query_size, 0);
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
void range_prover::range_prove(const ll * x,unsigned range,int m,int thread_num) {
    
    int log = 9; // range = 16 or 32
    //assert(range%log==0);
    int l=(range+log-1)/log;
    // 构造一个data矩阵l x m：m是x的元素个数
    ll** data=new ll*[l];
    for(int i=0;i<l;i++)
        data[i]=new ll[m];

    // 把x的每个元素拆分成log位，存入data矩阵，竖着存放
    for(int i=0;i<l;i++)
    {
        const int chunk_bits=std::min(log, static_cast<int>(range)-log*i);
        const ll chunk_mask=(static_cast<ll>(1)<<chunk_bits)-1;
        for(int j=0;j<m;j++)
        {
            data[i][j]=(x[j]>>(log*i))&chunk_mask;
        }
    }
    for(int i=0;i<l;i++)
    {
        const int chunk_bits=std::min(log, static_cast<int>(range)-log*i);
        const int table_size=1<<chunk_bits;
        std::cout << "  proving chunk " << i << " (" << chunk_bits
                  << " bits)" << std::endl;
        ll* t=new ll[table_size];
        for(int j=0;j<table_size;j++) t[j]=j;
        logup(data[i],t,m,table_size,thread_num);
        delete[] t;
        delete[] data[i];
    }
    delete[] data;
    printf("range proof time: commit time %.6f s, open time %.6f s,sumcheck_time %.6f s\n",range_proof_commit_time,range_proof_open_time,range_proof_sumcheck_time);
    printf("temp time: %.6f s\n",temp_time);
    
}
// 证明分片f[m]的所有元素都属于集合t[n]
void range_prover::logup(ll * f,ll *t,int m,int n,int thread)
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
    // Fr s1=0,s2=0;
    // for(int i=0;i<m;i++)
    //     s1+=G[i];
    // for(int i=0;i<n;i++)
    // {
    //     H[i]=H[i]*Fr(c[i]);
    //     s2+=H[i];
    // }
    temp_timer.stop();
    temp_time+=temp_timer.elapse_sec();

    range_proof_commit_timer.start();
    G1* f_comm=range_proof_prover_commit(f,g,lg2(m),thread);
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

}

double range_prover::prove() {
    if (!built_from_witness)
        throw std::logic_error("Range Proof must be built from the registered witness");
    verifyWitnessConsistency();
    double prover_time = 0;
    for (auto& op : ops) {
        for (auto& constraint : op.constraints) {
            cout << "start range prove"<<" query_size "<< constraint.query_size<<"\t";
            cout << " range_size "<< constraint.range_size << endl;
            prove_timer.start();
            range_prove(constraint.inputs.data(), constraint.range_size,
                        constraint.query_size, thread_num);
            cout << "end range prove" << endl;
            prove_timer.stop();
            prover_time += prove_timer.elapse_sec();
            cout << "time: " << prove_timer.elapse_sec() << endl;
        }
    }
    return prover_time;
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

ll range_prover::encodeWitnessValue(const F &field_value,
                                    const RangeConstraint &constraint,
                                    std::size_t absolute_offset) {
    const __int128 value=convert(field_value);
    const bool fits=constraint.is_signed
        ? fitsSignedBits(value, constraint.bits)
        : fitsUnsignedBits(value, constraint.bits);
    if (!fits) {
        std::ostringstream message;
        message << "Range Proof value out of bounds for " << constraint.name
                << " at val[0][" << absolute_offset << "]: value="
                << int128ToString(value) << ", bits=" << constraint.bits
                << ", signed=" << (constraint.is_signed ? "true" : "false");
        throw std::range_error(message.str());
    }
    if (!constraint.is_signed) return value;
    const __int128 bias=static_cast<__int128>(1)<<(constraint.bits-1);
    return value+bias;
}

void range_prover::buildFromWitness(const vector<F> &val0,
                                    const WitnessRegistry &registry) {
    registry.validateLayout(val0.size());
    validateShape(registry.shape());
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
        constraint.inputs.resize(padded, 0);
        constraint.query_size=static_cast<int>(padded);
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
            const ll expected=encodeWitnessValue(
                witness_source->at(region.val0_offset+i), constraint,
                region.val0_offset+i);
            if (proof_constraint.inputs.at(region.proof_start+i)!=expected) {
                std::ostringstream message;
                message << "Range Proof copy no longer matches val[0] for "
                        << region.name << " at relative index " << i;
                throw std::logic_error(message.str());
            }
        }
    }
}

void range_prover::tamperBuiltValueForTest(std::size_t region_index) {
    if (!built_from_witness)
        throw std::logic_error("cannot tamper an uninitialized Range Proof");
    const auto &region=query_regions.at(region_index);
    Constraint &constraint=ops.front().constraints.at(region.proof_constraint_index);
    constraint.inputs.at(region.proof_start)+=1;
}
