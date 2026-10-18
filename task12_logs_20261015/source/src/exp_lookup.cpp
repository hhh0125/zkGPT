#include "exp_lookup.hpp"
#include "range_logup.hpp"
#include "range_sumcheck.hpp"
#include "range_serialization.hpp"
#include "hyrax_opening.hpp"
#include "sparse_opening.hpp"
#include <cybozu/sha2.hpp>
#include <chrono>
#include <iostream>
#include <set>
namespace {
const std::uint32_t activeTable[]={
#include "exp_table_values.inc"
};
static_assert(sizeof(activeTable)/sizeof(activeTable[0])==kExpTailStart,"table size");
void require(bool b,const std::string &e) { if(!b) throw std::runtime_error(e); }
size_t log2exact(size_t n) { require(n && !(n&(n-1)),"exp power-of-two size");size_t l=0;while(n>1){n>>=1;++l;}return l; }
size_t padded(size_t n) {size_t p=1;while(p<n)p*=2;return p;}
std::vector<Fr> point(Transcript &t,size_t l) {std::vector<Fr> r;while(l--)r.push_back(t.challenge("exp.point"));return r;}
Fr mle(std::vector<Fr> v,const std::vector<Fr> &r) { for(auto &x:r){ for(size_t i=0;i<v.size()/2;i++)v[i]=v[2*i]*(Fr(1)-x)+v[2*i+1]*x;v.resize(v.size()/2);} require(v.size()==1,"exp MLE dimensions");return v[0]; }
void appendComm(Transcript &t,const std::vector<std::vector<std::vector<G1>>> &c){t.appendU64("vectors",c.size());for(auto &a:c){require(a.size()==1,"exp commitment layout");t.appendU64("rows",a[0].size());for(auto &g:a[0])t.appendG1("commitment",g);}}
RangePublicStatement sparseLayout(const RangePublicStatement &s,const std::vector<ExpQueryPosition> &q) {
    RangePublicStatement o=s; o.queries.clear();o.regions.clear();size_t n=padded(q.size());
    o.queries={{32,q.size(),n,{}},{32,q.size(),n,{}}};
    for(size_t col=0;col<2;col++)for(size_t i=0;i<q.size();) {
        size_t idx=col?q[i].E_witness_index:q[i].t_witness_index,k=1;
        while(i+k<q.size() && (col?q[i+k].E_witness_index:q[i+k].t_witness_index)==idx+k)++k;
        o.regions.push_back({WitnessKind::SOFTMAX,"exp",idx,k,32,false,col,i});i+=k;
    } return o;
}
Transcript start(const RangePublicStatement &s,const ExpLookupProof &p,const Fr &context) {
    Transcript t("zkGPT.exp-lookup.v4"); t.appendFr("bound-context",context);
    t.appendBytes("exp_table_id",p.exp_table_id.data(),32);t.appendU64("queries",p.query_count);
    appendRangeStatement(t,s);appendComm(t,p.payload.chunk_commitments);return t;
}
// The reused Range codec serializes every LogUp scalar even when this slot
// carries only a normalization sumcheck. MCL Fr's default constructor does
// not initialize storage; explicitly zero the unused canonical shell.
LogUpProof normalizationShell() {
    LogUpProof p;
    p.multiplicity_evaluation=Fr(0);p.reciprocal_sum=Fr(0);p.transcript_binding=Fr(0);
    p.reciprocal_table_sumcheck.final_f_evaluation=Fr(0);
    p.reciprocal_table_sumcheck.final_g_evaluation=Fr(0);
    p.reciprocal_value_sumcheck.final_f_evaluation=Fr(0);
    p.reciprocal_value_sumcheck.final_g_evaluation=Fr(0);
    p.reciprocal_value_equality.final_evaluation=Fr(0);
    p.reciprocal_table_equality.final_evaluation=Fr(0);
    for(auto *o:{&p.multiplicity_opening,&p.reciprocal_table_opening,
        &p.reciprocal_value_opening,&p.value_opening,
        &p.reciprocal_value_sum_opening,&p.reciprocal_table_sum_opening})
        o->ipa.final_witness=Fr(0);
    return p;
}
bool canonicalNormalization(const LogUpProof &p) {
    RangeProof actual,expected;actual.transcript_binding=Fr(0);expected.transcript_binding=Fr(0);
    actual.membership_proofs={{p}};
    auto shell=normalizationShell();shell.reciprocal_value_sumcheck=p.reciprocal_value_sumcheck;
    expected.membership_proofs={{shell}};
    return serializeRangeProof(actual)==serializeRangeProof(expected);
}
std::vector<Fr> rangeTable(unsigned bits) {std::vector<Fr> t(size_t{1}<<bits);for(size_t i=0;i<t.size();i++)t[i]=Fr(i);return t;}
std::vector<Fr> pairTable(const Fr &alpha) {std::vector<Fr> t(8192,Fr(kExpTailStart)+alpha);for(size_t i=0;i<kExpTailStart;i++)t[i]=Fr(i)+alpha*Fr(activeTable[i]);return t;}
std::vector<G1> linear(const std::vector<G1> &a,const std::vector<G1> &b,const Fr &scale) {require(a.size()==b.size(),"exp commitment rows");auto c=a;for(size_t i=0;i<c.size();i++)c[i]+=b[i]*scale;return c;}
}
std::uint32_t canonicalExpValue(std::uint64_t t){return t<kExpTailStart?activeTable[t]:1;}
ExpTableDigest canonicalExpTableId() {
    static const auto d=[] {
        cybozu::Sha256 h;const std::string spec="zkGPT.exp.table.v4;size=655360;t=uint32;floor(2^20*exp(-j/512)+1/2);max(value,1);tail=1;encoding=uint32LE;tail_start=6891";
        h.update(spec.data(),spec.size());for(size_t i=0;i<kExpTableSize;i++){uint32_t v=canonicalExpValue(i);uint8_t b[4];for(int j=0;j<4;j++)b[j]=v>>(8*j);h.update(b,4);}ExpTableDigest d;h.digest(d.data(),32,nullptr,0);return d;
    }();return d;
}
std::vector<ExpQueryPosition> expQueryLayout(const RangePublicStatement &s) {
    const auto &sh=s.shape; require(sh.sequence_length==32 && sh.layer_count>0 && sh.layer_count<=12 && sh.head_count>0 && sh.head_count<=12,"exp shape");
    size_t count=sh.head_count*sh.sequence_length*(sh.sequence_length+1)/2;
    std::vector<ExpQueryPosition> q;std::set<size_t> slots;
    for(size_t op=0;op<sh.layer_count;op++) {
        const PublicRangeRegion *tr=nullptr,*er=nullptr;
        for(const auto &r:s.regions) {
            if(r.name=="softmax_"+std::to_string(op)+"_t") {require(!tr,"duplicate t region");tr=&r;}
            if(r.name=="softmax_"+std::to_string(op)+"_E") {require(!er,"duplicate E region");er=&r;}
        }
        require(tr && er,"missing Softmax query region");require(tr->count==count && er->count==count && tr->bits==32 && er->bits==32 && !tr->is_signed && !er->is_signed && tr->kind==WitnessKind::SOFTMAX && er->kind==WitnessKind::SOFTMAX,"exp query layout metadata");
        require(er->val0_offset==tr->val0_offset+count,"exp query E offset");
        for(size_t i=0;i<count;i++){require(slots.insert(tr->val0_offset+i).second && slots.insert(er->val0_offset+i).second,"duplicate exp witness slot");q.push_back({op,i,tr->val0_offset+i,er->val0_offset+i});}
    }
    return q;
}
ExpLookupProof proveExpLookup(const RangePublicStatement &s,const std::vector<Fr> &w,const Fr &context,int threads,bool attack) {
    auto began=std::chrono::steady_clock::now();auto q=expQueryLayout(s);size_t n=padded(q.size()),l=log2exact(n);
    const auto &g=getGeneratorSet({"zkGPT/exp-lookup/v4",uint32_t(std::max(size_t{128},size_t{1}<<(l-l/2)))});
    ExpLookupProof p;p.exp_table_id=canonicalExpTableId();p.query_count=q.size();auto &b=p.payload;
    // Fixed vectors: original t,E; active selector s; normalized x; four u8 slack limbs.
    std::vector<std::vector<Fr>> v(8,std::vector<Fr>(n,Fr(0)));std::vector<ll> pc(8192,0);
    for(size_t i=0;i<n;i++) {
        uint64_t t=0,E=canonicalExpValue(0);if(i<q.size()) {t=w.at(q[i].t_witness_index).getInt64();E=w.at(q[i].E_witness_index).getInt64();}
        require(t<(1ULL<<32) && E<(1ULL<<32),"exp original query outside uint32");
        bool active=t<kExpTailStart;uint64_t x=active?t:kExpTailStart,d=active?kExpTailStart-1-t:t-kExpTailStart;
        v[0][i]=Fr(t);v[1][i]=Fr(E);v[2][i]=Fr(active?1:0);v[3][i]=Fr(x);for(int j=0;j<4;j++)v[4+j][i]=Fr((d>>(8*j))&255);
        ++pc[x];
    }
    for(auto &vec:v)b.chunk_commitments.push_back({commitLookupFields(vec,g.generators,threads)});
    auto t=start(s,p,context);Fr alpha=t.challenge("exp.pair.alpha");std::vector<Fr> pairs(n);
    for(size_t i=0;i<n;i++)pairs[i]=v[3][i]+alpha*v[1][i];
    b.membership_proofs.resize(1);
    b.membership_proofs[0].push_back(proveFieldLogUp(0,pairs,pairTable(alpha),pc,g.generators,g.u,threads,t.challenge("exp.logup.context"),attack));
    t.appendFr("exp.logup.binding",b.membership_proofs[0].back().transcript_binding);
    for(size_t col=2;col<8;col++){if(col==3)continue;unsigned bits=col==2?1:8;std::vector<ll> counts(size_t{1}<<bits,0);for(auto &a:v[col])++counts.at(a.getInt64());size_t k=b.membership_proofs[0].size();b.membership_proofs[0].push_back(proveFieldLogUp(k,v[col],rangeTable(bits),counts,g.generators,g.u,threads,t.challenge("exp.logup.context")));t.appendFr("exp.logup.binding",b.membership_proofs[0].back().transcript_binding);}
    auto open=[&](size_t col,const std::vector<Fr> &r){Fr e=mle(v[col],r);ReconstructionProof rp;rp.query_index=col;rp.encoded_evaluation=e;rp.encoded_chunk_opening=hyraxMleOpenProveFr(v[col],b.chunk_commitments[col][0],r,g.generators,g.u,e,t,"exp/vector/"+std::to_string(b.reconstruction_proofs.size()));b.reconstruction_proofs.push_back(std::move(rp));return e;};
    // Padding original t,E must be zero for sparse openings. Bind only an actual
    // query prefix with weights and subtract the known canonical padding E.
    auto r=point(t,l);auto sp=sparseLayout(s,q);
    Fr te=open(0,r),ee=open(1,r);auto pad=v[1];for(size_t i=0;i<q.size();i++)pad[i]=Fr(0);Fr padE=mle(pad,r);
    b.val0_openings.push_back(proveSparseVal0Opening(sp,0,r,w,te,t));b.val0_openings.push_back(proveSparseVal0Opening(sp,1,r,w,ee-padE,t));
    // s(t-K)=x-K ; s(2K-1-2t)=d-t+K. Boolean s and u32 d
    // force the correct branch, including all uint32 tail indices.
    for(size_t relation=0;relation<2;relation++) {
        auto r=point(t,l);Fr claim;
        if(relation==0)claim=open(3,r)-Fr(kExpTailStart);
        else {Fr d=0,power=1;for(size_t j=4;j<8;j++){d+=power*open(j,r);power*=Fr(256);}claim=d-open(0,r)+Fr(kExpTailStart);}
        std::vector<Fr> f=v[0];for(auto &a:f)a=relation?Fr(2*kExpTailStart-1)-Fr(2)*a:a-Fr(kExpTailStart);
        LogUpProof sc=normalizationShell();std::vector<Fr> end;sc.reciprocal_value_sumcheck=proveDegree3Sumcheck(r,v[2],f,claim,t,"exp/normalization/"+std::to_string(relation),&end);b.membership_proofs[0].push_back(sc);
        open(2,end);open(0,end);
    }
    b.transcript_binding=t.challenge("exp.final");
    std::cout<<"Exp Lookup query count: "<<q.size()<<"; table size: 8192 (canonical entries 655360); padded queries: "<<n<<"\nExp Lookup Prover time: "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count()<<"s\nExp Lookup core vector workspace bytes: "<<8*n*sizeof(Fr)<<" (excludes kernels and sparse workspace)\n";
    return p;
}
bool verifyExpLookup(const RangePublicStatement &s,const ExpLookupProof &p,const Fr &context,std::string *error) {
    try {
        auto began=std::chrono::steady_clock::now();require(p.exp_table_id==canonicalExpTableId(),"unexpected exp_table_id");auto q=expQueryLayout(s);require(p.query_count==q.size(),"exp query count mismatch");size_t n=padded(q.size()),l=log2exact(n);
        const auto &g=getGeneratorSet({"zkGPT/exp-lookup/v4",uint32_t(std::max(size_t{128},size_t{1}<<(l-l/2)))});auto &b=p.payload;
        require(b.chunk_commitments.size()==8 && b.membership_proofs.size()==1 && b.membership_proofs[0].size()==8 && b.val0_openings.size()==2 && b.reconstruction_proofs.size()==12,"exp proof layout");
        for(auto &c:b.chunk_commitments)require(c.size()==1 && c[0].size()==(size_t{1}<<(l/2)),"exp commitment dimensions");
        auto t=start(s,p,context);Fr alpha=t.challenge("exp.pair.alpha");std::string e;
        auto verifyLog=[&](size_t k,const std::vector<Fr> &table,const std::vector<G1> &comm){require(verifyFieldLogUp(n,k,table,comm,b.membership_proofs[0][k],g.generators,g.u,t.challenge("exp.logup.context"),&e),e);t.appendFr("exp.logup.binding",b.membership_proofs[0][k].transcript_binding);};
        verifyLog(0,pairTable(alpha),linear(b.chunk_commitments[3][0],b.chunk_commitments[1][0],alpha));size_t k=1;for(size_t col=2;col<8;col++){if(col==3)continue;verifyLog(k++,rangeTable(col==2?1:8),b.chunk_commitments[col][0]);}
        size_t oi=0;auto open=[&](size_t col,const std::vector<Fr> &r){auto &rp=b.reconstruction_proofs.at(oi);require(rp.query_index==col,"exp opening order");require(hyraxMleOpenVerify(b.chunk_commitments[col][0],r,g.generators,g.u,rp.encoded_evaluation,rp.encoded_chunk_opening,t,"exp/vector/"+std::to_string(oi++),&e),e);return rp.encoded_evaluation;};
        auto r=point(t,l);auto sp=sparseLayout(s,q);Fr te=open(0,r),ee=open(1,r);std::vector<Fr> pad(n,Fr(0));for(size_t i=q.size();i<n;i++)pad[i]=Fr(canonicalExpValue(0));Fr padE=mle(pad,r);
        require(verifySparseVal0Opening(sp,0,r,te,b.val0_openings[0],t,&e),e);require(verifySparseVal0Opening(sp,1,r,ee-padE,b.val0_openings[1],t,&e),e);
        for(size_t relation=0;relation<2;relation++) {
            auto r=point(t,l);Fr claim;if(relation==0)claim=open(3,r)-Fr(kExpTailStart);else{Fr d=0,power=1;for(size_t j=4;j<8;j++){d+=power*open(j,r);power*=Fr(256);}claim=d-open(0,r)+Fr(kExpTailStart);}
            require(canonicalNormalization(b.membership_proofs[0][6+relation]),"noncanonical exp normalization payload");
            auto &sc=b.membership_proofs[0][6+relation].reciprocal_value_sumcheck;require(sc.rounds.size()==l,"exp normalization rounds");std::vector<Fr> end;
            require(verifyDegree3Sumcheck(r,claim,sc,t,"exp/normalization/"+std::to_string(relation),&end,&e),e);Fr se=open(2,end),tv=open(0,end);require(sc.final_f_evaluation==se && sc.final_g_evaluation==(relation?Fr(2*kExpTailStart-1)-Fr(2)*tv:tv-Fr(kExpTailStart)),"exp normalization opening relation");
        }
        require(oi==b.reconstruction_proofs.size(),"exp extra opening");require(b.transcript_binding==t.challenge("exp.final"),"exp transcript binding");
        std::cout<<"Exp Lookup Verifier time: "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count()<<"s; queries="<<q.size()<<"\n";return true;
    }catch(const std::exception &e){if(error)*error=e.what();return false;}
}
