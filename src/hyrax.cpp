#include "hyrax.hpp"
#include "timer.hpp"
#include <cmath>
#include <thread>
#include <vector>
#include <mutex>
#include <iostream>
#include <chrono>
#include <atomic>
#include <exception>
using namespace std;
using namespace mcl::bn;

const int MAX_MSM_LEN=1e4;
const int COMM_OPT_MAX=65536; //don't optimize if larger than this  2^16
const int logmax=16;  /// max number=2^18-1
const int block_num=8;  // Full signed/unsigned 128-bit scalar decomposition.


G1 perdersen_commit(G1* g,ll* f,int n,G1* W)
{
    G1 ret;
    ret.clear();
    //timer t(true);
    //t.start();
    
    bool *used=new bool[COMM_OPT_MAX*block_num];
    memset(used,0,sizeof(bool)*COMM_OPT_MAX*block_num);
    unsigned __int128 bar[block_num]; // bar[i]=2^(16*i)
    unsigned __int128 bar_t=1;
    for(int i=0;i<block_num;i++)
    {
        bar[i]=bar_t;
        bar_t<<=logmax;
    }
    // 将f[i]拆分成多个16bit的块 
    for(int i=0;i<n;i++)
    {
            if(f[i]==0)
                continue;
            
            if(f[i]<0)
            {
                const unsigned __int128 tmp=
                    static_cast<unsigned __int128>(-(f[i]+1))+1;
                for(int j=0;j<block_num;j++)
                {
                    if(tmp<bar[j])
                        break;
                    const std::size_t fnow=static_cast<std::size_t>(
                        (tmp>>(logmax*j))&65535);
                    W[fnow+(j<<logmax)]-=g[i];
                    used[fnow+(j<<logmax)]=1;
                }
            }
            else
            {
                const unsigned __int128 tmp=
                    static_cast<unsigned __int128>(f[i]);
                for(int j=0;j<block_num;j++)
                {
                    if(tmp<bar[j])
                        break;
                    const std::size_t fnow=static_cast<std::size_t>(
                        (tmp>>(logmax*j))&65535);
                    W[fnow+(j<<logmax)]+=g[i];
                    used[fnow+(j<<logmax)]=1;
                }
            }
    }
    G1 gg[logmax*block_num];
    for(int j=0;j<logmax*block_num;j++)
        gg[j].clear();
    for(int j=0;j<COMM_OPT_MAX*block_num;j++)
    {
        if(used[j])
        {
            int jj=j%COMM_OPT_MAX;
            int blk=j/COMM_OPT_MAX;
            for(int k=0;k<logmax;k++)
            {
                if(jj&(1<<k))
                    gg[k+logmax*blk]+=W[j];
            }
            W[j].clear();
            used[j]=0;            
        }
    }
    for(int j=0;j<logmax*block_num;j++) {
        G1 weighted=gg[j];
        for(int bit=0;bit<j;++bit) weighted+=weighted;
        ret+=weighted;
    }
    delete []used;
    return ret;
}


G1 perdersen_commit(G1* g,int* f,int n,G1* W)
{
    G1 ret;
    ret.clear();
    bool *used=new bool[COMM_OPT_MAX];
    memset(used,0,sizeof(bool)*COMM_OPT_MAX);
    for(int i=0;i<n;i++)
    {
            if(f[i]==0)
                continue;
            
            if(f[i]<0)
            {
                const i64 index=-static_cast<i64>(f[i]);
                if (index>=COMM_OPT_MAX)
                    throw std::range_error("integer commitment scalar is too large");
                W[index]-=g[i];
                used[index]=1;
            }
            else
            {
                if (f[i]>=COMM_OPT_MAX)
                    throw std::range_error("integer commitment scalar is too large");
                W[f[i]]+=g[i];
                used[f[i]]=1;
            }
    }
    //t.stop("add ",false);
    const int logn=log2(COMM_OPT_MAX)+1;
    G1 gg[40];
    for(int j=0;j<logn;j++)
        gg[j].clear();
    for(int j=1;j<COMM_OPT_MAX;j++)
    {
        if(used[j])
        {
            for(int k=0;k<logn;k++)
            {
                if(j&(1<<k))
                    gg[k]+=W[j];
            }
            W[j].clear();
            used[j]=0;            
        }
    }
    for(int j=0;j<logn;j++)
        ret+=gg[j]*(1<<j);

    //t.stop("accu",false);
    //t.stop("ALL: ",true);

    delete []used;
    return ret;
}

G1 perdersen_commit(G1* g,Fr* f,int n)
{
    G1 ret;
    G1::mulVec(ret,g,f,n);
    return ret;
}

Fr lagrange(Fr *r,int l,int k)
{
    if (l<0 || l>30 || k<0 || k>=(1<<l))
        throw std::out_of_range("Hyrax Lagrange index is out of bounds");
    Fr ret=1;
    for(int i=0;i<l;i++)
    {
        if(k&(1<<i))
            ret*=r[i];
        else
            ret*=1-r[i];
    }
    return ret;
}
// 计算拉格朗日插值
void brute_force_compute_LR(Fr* L,Fr* R,Fr* r,int l)
{
    int halfl=l/2,c=l-halfl;
    for(int k=0;k<(1<<c);k++)
        L[k]=lagrange(r,c,k);
    for(int k=0;k<(1<<halfl);k++)
        R[k]=lagrange(r+c,halfl,k);
}

Fr brute_force_compute_eval(Fr* w,Fr* r,int l)
{
    Fr ret=0;
    for(int k=0;k<(1<<l);k++)
        ret+=lagrange(r,l,k)*w[k];
    return ret;
}


G1 compute_Tprime(int l,Fr* R,G1* Tk) 
{
    //w has 2^l length
    //assert(l%2==0);
    int halfl=l/2;
    int rownum=(1<<halfl),colnum=(1<<(l-halfl));
    G1 ret=perdersen_commit(Tk,R,rownum);
    return ret;
}

G1 compute_RT_singlethread(Fr*w ,Fr*R,int l,G1*g,Fr*& ret) // L is row number length
{
    int halfl=l/2;
    int rownum=(1<<halfl),colnum=(1<<(l-halfl));
    Fr* res=new Fr[colnum];
    for(int i=0;i<colnum;i++)
        res[i]=0;
    timer TT;
    TT.start();
    for(int j=0;j<colnum;j++)
    for(int i=0;i<rownum;i++)
    {
        if(!w[j+i*colnum].isZero())
            res[j]+=R[i]*w[j+i*colnum];  // mat mult  (1,row)*(row,col)=(1,col)
    }
    TT.stop();
    G1 comm=perdersen_commit(g,res,colnum);
    ret=res;
    return comm;
}


G1 compute_RT(Fr *w, Fr *R, int l, G1 *g, Fr *&ret) // L is row number length
{
    int halfl = l / 2;
    int rownum = (1 << halfl), colnum = (1 << (l - halfl));
    Fr *res = new Fr[colnum];
    for (int i = 0; i < colnum; i++)
        res[i] = 0;

    auto start = std::chrono::high_resolution_clock::now();

    // Define the number of threads
    const int num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    std::mutex res_mutex;

    auto worker = [&](int start_col, int end_col) {
        Fr *local_res = new Fr[colnum]; // Thread-local result storage
        for (int i = start_col; i < end_col; i++)
            local_res[i] = 0;

        for (int j = start_col; j < end_col; j++) {
            for (int i = 0; i < rownum; i++) {
                if (!w[j + i * colnum].isZero()) {
                    local_res[j] += R[i] * w[j + i * colnum];
                }
            }
        }

        // Merge local results into global result
        std::lock_guard<std::mutex> guard(res_mutex);
        for (int j = start_col; j < end_col; j++) {
            res[j] += local_res[j];
        }

        delete[] local_res; // Free thread-local storage
    };

    // Split columns into chunks for each thread
    int chunk_size = (colnum + num_threads - 1) / num_threads;
    for (int t = 0; t < num_threads; t++) {
        int start_col = t * chunk_size;
        int end_col = std::min(start_col + chunk_size, colnum);
        if (start_col < colnum) { // Avoid starting threads if no work is left
            threads.emplace_back(worker, start_col, end_col);
        }
    }
    // Join threads
    for (auto &th : threads) {
        if (th.joinable())
            th.join();
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    // Commit the result
    G1 comm = perdersen_commit(g, res, colnum);
    ret = res;
    return comm;
}

G1 gen_gi(G1* g,int n)
{
    if (g==nullptr || n<=0)
        throw std::invalid_argument("invalid Hyrax generator request");
    for(int i=0;i<n;i++) {
        const std::string label="zkGPT/main/g/"+std::to_string(i);
        hashAndMapToG1(g[i], label);
        if (!g[i].isValid() || g[i].isZero())
            throw std::runtime_error("invalid deterministic Hyrax generator");
    }
    G1 u;
    hashAndMapToG1(u, "zkGPT/main/u");
    if (!u.isValid() || u.isZero())
        throw std::runtime_error("invalid deterministic Hyrax U generator");
    return u;
}

namespace {

std::pair<bool, unsigned __int128> fieldSignedMagnitude(const Fr &value)
{
    const bool negative=value.isNegative();
    const Fr magnitude_field=negative ? -value : value;
    uint8_t bytes[16]={0};
    const int size=magnitude_field.getLittleEndian(bytes, sizeof(bytes));
    if (size<=0)
        throw std::range_error("Hyrax scalar exceeds signed 128-bit range");
    unsigned __int128 magnitude=0;
    for(int i=size-1;i>=0;--i) magnitude=magnitude*256+bytes[i];
    const unsigned __int128 sign_bit=static_cast<unsigned __int128>(1)<<127;
    if ((!negative && magnitude>=sign_bit) ||
        (negative && magnitude>sign_bit))
        throw std::range_error("Hyrax scalar exceeds signed 128-bit range");
    return {negative, magnitude};
}

G1 perdersen_commit(G1* g, Fr* f, int n, G1* W)
{
    G1 result;
    result.clear();
    bool *used=new bool[COMM_OPT_MAX*block_num];
    memset(used, 0, sizeof(bool)*COMM_OPT_MAX*block_num);

    for(int i=0;i<n;++i) {
        const auto scalar=fieldSignedMagnitude(f[i]);
        unsigned __int128 remaining=scalar.second;
        for(int block=0;block<block_num && remaining!=0;
            ++block, remaining>>=logmax) {
            const std::size_t digit=static_cast<std::size_t>(remaining&65535);
            if (digit==0) continue;
            const std::size_t bucket=digit+(block<<logmax);
            if (scalar.first) W[bucket]-=g[i];
            else W[bucket]+=g[i];
            used[bucket]=true;
        }
    }

    G1 bit_sums[logmax*block_num];
    for(G1 &sum : bit_sums) sum.clear();
    for(int bucket=0;bucket<COMM_OPT_MAX*block_num;++bucket) {
        if (!used[bucket]) continue;
        const int digit=bucket%COMM_OPT_MAX;
        const int block=bucket/COMM_OPT_MAX;
        for(int bit=0;bit<logmax;++bit)
            if (digit&(1<<bit)) bit_sums[bit+logmax*block]+=W[bucket];
        W[bucket].clear();
    }
    for(int bit=0;bit<logmax*block_num;++bit) {
        G1 weighted=bit_sums[bit];
        for(int shift=0;shift<bit;++shift) weighted+=weighted;
        result+=weighted;
    }
    delete[] used;
    return result;
}

}  // namespace

double blt_vtime=0;
Pack bullet_reduce(G1 gamma, Fr*a,G1*g,int n,G1& G,Fr* x,Fr y,bool need_free) // length n
{
    timer vtimer;
    if(n==1)
    {
        Pack p(gamma,a[0],g[0],x[0],y);
        return p;
    }
    
    //step2  prover fold
    G1 gamma_minus1,gamma_1;
    Fr x1a2=0,x2a1=0;
    for(int i=0;i<n/2;i++)
    {
        x1a2+=x[i]*a[n/2+i];
        x2a1+=x[n/2+i]*a[i];
    }
    gamma_minus1=G*x1a2+perdersen_commit(g+n/2,x,n/2);
    gamma_1=G*x2a1+perdersen_commit(g,x+n/2,n/2);
    Fr c,invc;
    c.setByCSPRNG();  // step3 V choose random c
    //prover verifier both comp
    vtimer.start();
    Fr::inv(invc,c);
    G1 gamma_prime=gamma_minus1*c*c+gamma_1*invc*invc+gamma;
    Fr* aprime=new Fr[n/2];       
    for(int i=0;i<n/2;i++)
        aprime[i]=a[i]*invc+a[i+n/2]*c;
    G1* gprime=new G1[n/2];        
    if(n<2048)
    {   
        for(int i=0;i<n/2;i++)
            gprime[i]=g[i]*invc+g[i+n/2]*c;
    }
    else  
    {
        int num_threads=16;
        int chunk_size = n/2/num_threads;
        std::vector<std::thread> threads1;
        auto worker1 = [&](int start_id, int end_id) 
        {
            for(int i=start_id;i<end_id;i++)
                gprime[i]=g[i]*invc+g[i+n/2]*c;
        };
        for (int t = 0; t < num_threads; t++) 
        {
            int start_col = t * chunk_size;
            int end_col = min(start_col + chunk_size, n/2);
            threads1.emplace_back(worker1, start_col, end_col);
        }
        for (auto &th : threads1) 
        {
            if (th.joinable())
                th.join();
        }
    }
    vtimer.stop();

    blt_vtime+=vtimer.elapse_sec();

    //prover single compute
    Fr* xprime=new Fr[n/2];         
    Fr yprime;
    for(int i=0;i<n/2;i++)
        xprime[i]=c*x[i]+invc*x[i+n/2];
    yprime=c*c*x1a2+invc*invc*x2a1+y;

    if(need_free)
    {
        delete []a;
        delete []g;
        delete []x;
    }
    
    return bullet_reduce(gamma_prime,aprime,gprime,n/2,G,xprime,yprime,true);
}   

void prove_dot_product(G1 comm_x, G1 comm_y, Fr* a, G1*g ,G1& G,Fr* x,Fr y,int n)  // y= <a,x> , 
{
    G1 gamma=comm_x+comm_y;
    Pack p=bullet_reduce(gamma,a,g,n,G,x,y);
    if (p.y!=p.x*p.a)
        throw std::runtime_error("Hyrax inner-product scalar check failed");
    if (p.gamma!=p.g*p.x+G*p.y)
        throw std::runtime_error("Hyrax inner-product commitment check failed");
}
static ThreadSafeQueue<int> workerq,endq;


void ll_commit_worker(G1*& Tk,G1*& g, ll*& row,int colnum,G1*& W)
{
    int idx;
    while (true)
    {
            bool ret=workerq.TryPop(idx);
            if(ret==false)
                return;
            Tk[idx]=perdersen_commit(g,row+idx*colnum,colnum,W);
            endq.Push(idx);
    }
}
G1* prover_commit(ll* w, G1* g, int l,int thread_n) //compute Tk, int version with pippenger
{
    int halfl=l/2;
    int rownum=(1<<halfl),colnum=(1<<(l-halfl));
    G1 *Tk=new G1[rownum];
    G1** W=new G1*[thread_n];
    for(int i=0;i<thread_n;i++)
        W[i]=new G1[COMM_OPT_MAX*block_num];
    for(int i=0;i<thread_n;i++)
        memset(W[i],0,sizeof(G1)*COMM_OPT_MAX*block_num);
    for (u64 i = 0; i < rownum; ++i)  //work for rownum 
        workerq.Push(i);
    vector<thread> workers;
    workers.reserve(thread_n);
    for(int i=0;i<thread_n;i++)
        workers.emplace_back(ll_commit_worker,std::ref(Tk),std::ref(g),
                             std::ref(w),colnum,std::ref(W[i]));
    for (auto &worker : workers) worker.join();
    if (endq.Size()!=static_cast<std::size_t>(rownum))
        throw std::runtime_error("Hyrax commitment worker count mismatch");
    endq.Clear();
    if (!workerq.Empty() || endq.Size()!=0)
        throw std::runtime_error("Hyrax commitment worker queue did not drain");
    for(int i=0;i<thread_n;i++)
        delete [] W[i];
    delete []W;
    return Tk;
}

G1* prover_commit(Fr* w, G1* g, int l, int thread_n)
{
    if (w==nullptr || g==nullptr || l<0 || l>30 || thread_n<=0)
        throw std::invalid_argument("invalid field Hyrax commitment request");
    const int rownum=1<<(l/2);
    const int colnum=1<<(l-l/2);
    G1 *commitments=new G1[rownum];
    const int worker_count=std::min(thread_n, rownum);
    std::atomic<int> next_row{0};
    std::atomic<bool> failed{false};
    std::exception_ptr worker_error;
    std::mutex error_mutex;
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    for(int worker=0;worker<worker_count;++worker) {
        workers.emplace_back([&] {
            G1 *buckets=new G1[COMM_OPT_MAX*block_num];
            memset(buckets, 0, sizeof(G1)*COMM_OPT_MAX*block_num);
            while(!failed.load(std::memory_order_relaxed)) {
                const int row=next_row.fetch_add(1);
                if (row>=rownum) break;
                try {
                    commitments[row]=perdersen_commit(
                        g, w+row*colnum, colnum, buckets);
                } catch (...) {
                    failed.store(true, std::memory_order_relaxed);
                    std::lock_guard<std::mutex> lock(error_mutex);
                    if (!worker_error) worker_error=std::current_exception();
                }
            }
            delete[] buckets;
        });
    }
    for(auto &worker : workers) worker.join();
    if (worker_error) {
        delete[] commitments;
        std::rethrow_exception(worker_error);
    }
    return commitments;
}



Fr prover_evaluate(Fr*w ,Fr*r,G1& G,G1* g, Fr*L,Fr*R,int l)  // nlogn brute force 
{
    int halfl=l/2;
    int rownum=(1<<halfl),colnum=(1<<(l-halfl));
    timer t(true);
    t.start();
    brute_force_compute_LR(L,R,r,l);
    Fr eval=brute_force_compute_eval(w,r,l);
    t.stop("eval total ",true,false);
    return eval;
}
namespace hyrax
{
pair<double,double> open(Fr*w,Fr*r,Fr eval,G1&G,G1*g,Fr*L,Fr*R,G1*tk,int l)
{
    double prover_time=0,verifier_time=0;
    int halfl=l/2;
    int rownum=(1<<halfl),colnum=(1<<(l-halfl));
    timer verf;
    verf.start();
    Fr* RT=new Fr[colnum];
    compute_RT(w,R,l,g,RT);  
    uint8_t bf[16]={0};	 //64 bit
    // printf("R_bitlen=%d,R[0].size=%d,%d,%d,%d\n",Fr::getBitSize(),R[0].getLittleEndian(bf,16),R[2],R[3],R[4]);
    // printf("RT=%d,%d,%d,%d,%d\n",RT[0],RT[1],RT[2],RT[3],RT[4]);
    // printf("w=%d,%d,%d,%d,%d\n",w[0],w[1],w[2],w[3],w[4]);
    verf.stop();
    prover_time+=verf.elapse_sec();
    verf.start();
    G1 tprime=compute_Tprime(l,R,tk);
    verf.stop();
    prover_time+=verf.elapse_sec();
    verifier_time+=verf.elapse_sec();
    verf.start();
    prove_dot_product(tprime, G*eval, L, g , G,RT,eval,colnum);
    delete[] RT;
    verf.stop();
    prover_time+=verf.elapse_sec();
    verifier_time+=blt_vtime;
    return make_pair(prover_time,verifier_time);
}
}
