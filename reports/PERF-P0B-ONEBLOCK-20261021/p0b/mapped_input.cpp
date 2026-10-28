#include <chrono>
#include <ctime>
#include <iostream>
#include <iomanip>
struct P0BDiagHits {unsigned long long singleton=0,contiguous=0,fallback=0;std::size_t cache_peak_bytes=0;};
static thread_local P0BDiagHits p0b_diag_hits;
static thread_local unsigned long long p0b_diag_calls=0;
#define P0B_KERNEL_HIT(kind) (++p0b_diag_hits.kind)
#define P0B_KERNEL_CACHE(bytes) (p0b_diag_hits.cache_peak_bytes=std::max(p0b_diag_hits.cache_peak_bytes,std::size_t(bytes)))
#include "mapped_input.hpp"
#include <algorithm>
namespace zkgpt::r1 {
void MappedInputSpec::validate() const {
    master.spec.validate();
    require(!context.empty() && context.size()<=4096,"mapped public context");
    require(!children.empty() && children.size()<=65536,"mapped child count");
    require(public_values.size()<=kMaxVector,"mapped public count");
    for (const auto &c:children) {
        c.commitment.spec.validate();
        require(!c.spans.empty() && c.spans.size()<=kMaxVector,"mapped span count");
        std::size_t count=0;
        for (const auto &span:c.spans) {
            require(span.count>0 && span.offset<master.spec.actual &&
                span.count<=master.spec.actual-span.offset,"mapped source bounds");
            require(count<=c.commitment.spec.actual && span.count<=c.commitment.spec.actual-count,
                "mapped child length");
            count+=span.count;
        }
        require(count==c.commitment.spec.actual,"mapped child coverage");
    }
    for (const auto &v:public_values) require(v.index<master.spec.actual,"mapped public index");
}
void appendMappedInput(const MappedInputSpec &s,Fs &t) {
    s.validate();
    t.text("mapped.protocol","affine-gather-input-sumcheck-v1");
    t.text("mapped.context",s.context); append(t,s.master);
    t.integer("mapped.children",s.children.size());
    for (const auto &c:s.children) {
        append(t,c.commitment);t.integer("mapped.spans",c.spans.size());
        for (const auto &v:c.spans) {
            t.integer("mapped.offset",v.offset);t.integer("mapped.count",v.count);
            t.scalar("mapped.bias",v.bias);
        }
    }
    t.integer("mapped.public",s.public_values.size());
    for (const auto &v:s.public_values) {
        t.integer("mapped.public.index",v.index);t.scalar("mapped.public.value",v.value);
    }
}
void appendCheckedInput(const MappedInputSpec &s,const std::vector<CheckedInputClaim> &claims,Fs &t) {
    require(claims.size()<=65536,"checked input claim count");
    if(claims.empty())return; // Preserve the standalone R1 extension transcript.
    t.text("mapped.checked.protocol","locally-verified-GKR-input-claims-v1");
    t.integer("mapped.checked.count",claims.size());
    for(const auto &c:claims){
        require(c.point.size()<=28 && c.spans.size()<=kMaxVector,"checked input dimension/spans");
        t.integer("mapped.checked.dimension",c.point.size());for(const auto &x:c.point)t.scalar("mapped.checked.point",x);
        t.scalar("mapped.checked.value",c.value);t.integer("mapped.checked.spans",c.spans.size());
        std::size_t size=0,limit=std::size_t(1)<<c.point.size();
        for(const auto &span:c.spans){
            require(span.count>0 && span.offset<s.master.spec.padded && span.count<=s.master.spec.padded-span.offset && span.bias.isZero(),"checked input source bounds/bias");
            require(size<=limit && span.count<=limit-size,"checked input padded length");size+=span.count;
            t.integer("mapped.checked.offset",span.offset);t.integer("mapped.checked.length",span.count);
        }
    }
}
namespace {
Fr eqIndex(const std::vector<Fr> &r,std::size_t i) {
    Fr v=1;
    for (unsigned bit=0;bit<r.size();++bit) v*=((i>>bit)&1) ? r[bit] : Fr(1)-r[bit];
    return v;
}
Fr prefixWeight(const std::vector<Fr> &r,std::size_t end) {
    if(end==(std::size_t(1)<<r.size()))return Fr(1);
    Fr below=0,prefix=1;
    for(unsigned bit=r.size();bit-->0;) {
        if((end>>bit)&1) {below+=prefix*(Fr(1)-r[bit]);prefix*=r[bit];}
        else prefix*=Fr(1)-r[bit];
    }
    return below;
}
// Sum eq(child,child_start+z)*eq(master,source_start+z), 0<=z<count.
// Binary addition carries and comparison with count give a constant-size
// digit DP. Complexity depends on dimensions/spans, not their scalar count,
// including odd offsets where dyadic common-alignment splitting degenerates.
Fr spanKernelDP(const std::vector<Fr> &child,std::size_t child_start,
    const std::vector<Fr> &master,std::size_t source_start,std::size_t count) {
    const unsigned d=std::max(child.size(),master.size());
    std::array<std::array<std::array<Fr,3>,2>,2> state{};
    auto clear=[](auto &values) {for(auto &a:values)for(auto &b:a)for(auto &v:b)v=Fr(0);};
    clear(state); // MCL Fr's default constructor leaves its limbs uninitialized.
    state[0][0][1]=Fr(1);
    for(unsigned bit=0;bit<=d;++bit) {
        decltype(state) next{};
        clear(next);
        for(unsigned ca=0;ca<2;++ca)for(unsigned cb=0;cb<2;++cb)
            for(unsigned cmp=0;cmp<3;++cmp)for(unsigned z=0;z<(bit==d?1U:2U);++z) {
                auto av=z+unsigned((child_start>>bit)&1)+ca;
                auto bv=z+unsigned((source_start>>bit)&1)+cb;
                Fr weight=state[ca][cb][cmp];
                if(bit<child.size())weight*=av&1 ? child[bit] : Fr(1)-child[bit];
                else if(av&1)continue;
                if(bit<master.size())weight*=bv&1 ? master[bit] : Fr(1)-master[bit];
                else if(bv&1)continue;
                auto bound=unsigned((count>>bit)&1);
                auto comparison=z<bound?0U:z>bound?2U:cmp;
                next[av/2][bv/2][comparison]+=weight;
            }
        state=std::move(next);
    }
    return state[0][0][0];
}
// Diagnostic hooks are absent from normal builds; no mutable cache is global.
#ifndef P0B_KERNEL_HIT
#define P0B_KERNEL_HIT(kind) ((void)0)
#endif
#ifndef P0B_KERNEL_CACHE
#define P0B_KERNEL_CACHE(bytes) ((void)0)
#endif
class EqWeights {
    std::vector<Fr> point_,low_,high_;
    unsigned split_=0;
public:
    explicit EqWeights(const std::vector<Fr> &p) {
        require(p.size()<=28,"mapped equality dimension");
        point_=p; // Own exact coordinates: no address key, alias, or stale view.
        split_=unsigned(point_.size()/2);
        low_=weights(std::vector<Fr>(point_.begin(),point_.begin()+split_));
        high_=weights(std::vector<Fr>(point_.begin()+split_,point_.end()));
    }
    const std::vector<Fr> &point() const {return point_;}
    std::size_t domain() const {return std::size_t(1)<<point_.size();}
    std::size_t storageBytes() const {return (low_.capacity()+high_.capacity())*sizeof(Fr);}
    Fr at(std::size_t index) const {
        if(index>=domain())require(false,"mapped equality index");
        return low_[index&(low_.size()-1)]*high_[index>>split_];
    }
    Fr above(std::size_t index,unsigned first_bit) const {
        if(first_bit==0)return eqIndex(point_,index);
        Fr out=1;
        for(unsigned bit=first_bit;bit<point_.size();++bit)
            out*=((index>>bit)&1)?point_[bit]:Fr(1)-point_[bit];
        return out;
    }
};
class SpanEvaluator {
    const EqWeights child_;
    const EqWeights &master_;
    std::vector<Fr> low_pair_;
public:
    SpanEvaluator(const std::vector<Fr>&child,const EqWeights &master)
        :child_(child),master_(master) {
        const auto n=std::min(child_.point().size(),master_.point().size());
        low_pair_.assign(n+1,Fr(1));
        for(unsigned bit=0;bit<n;++bit) {
            const auto &a=child_.point()[bit],&b=master_.point()[bit];
            low_pair_[bit+1]=low_pair_[bit]*((Fr(1)-a)*(Fr(1)-b)+a*b);
        }
        P0B_KERNEL_CACHE(child_.storageBytes()+master_.storageBytes());
    }
    Fr value(std::size_t child_start,std::size_t source_start,std::size_t count) const {
        if(count==0){P0B_KERNEL_HIT(contiguous);return Fr(0);}
        const auto fallback=[&] {
            P0B_KERNEL_HIT(fallback);
            return spanKernelDP(child_.point(),child_start,master_.point(),source_start,count);
        };
        // Public append/validation already enforces these bounds. Keep the
        // original DP behavior for unsupported private test ranges as well.
        if(child_start>=child_.domain() || source_start>=master_.domain() ||
           count>child_.domain()-child_start || count>master_.domain()-source_start)
            return fallback();
        if(count==1) {
            P0B_KERNEL_HIT(singleton);
            return child_.at(child_start)*master_.at(source_start);
        }
        constexpr std::size_t direct_limit=16,block_limit=32;
        if(count<=direct_limit) {
            P0B_KERNEL_HIT(contiguous);Fr out=0;
            for(std::size_t z=0;z<count;++z)out+=child_.at(child_start+z)*master_.at(source_start+z);
            return out;
        }
        if((child_start^source_start)&1)return fallback();
        struct Block {std::size_t child,master;unsigned bits;};
        std::array<Block,block_limit>blocks{};
        std::size_t used=0,left=count,a=child_start,b=source_start;
        while(left && used<block_limit) {
            unsigned bits=0;for(auto v=left;v>1;v>>=1)++bits;
            std::size_t step=std::size_t(1)<<bits;
            while(step>1 && ((a|b)&(step-1))){step>>=1;--bits;}
            blocks[used++]={a,b,bits};a+=step;b+=step;left-=step;
        }
        if(left)return fallback(); // Work is bounded even for bad common alignment.
        P0B_KERNEL_HIT(contiguous);Fr out=0;
        for(std::size_t j=0;j<used;++j) {
            const auto &block=blocks[j];
            out+=low_pair_[block.bits]*child_.above(block.child,block.bits)*master_.above(block.master,block.bits);
        }
        return out;
    }
};
#ifdef P0B_KERNEL_TEST_API
#define P0B_KERNEL_PREPARED_TESTS 1
Fr spanKernel(const std::vector<Fr>&child,std::size_t child_start,
    const std::vector<Fr>&master,std::size_t source_start,std::size_t count) {
    const EqWeights cache(master);return SpanEvaluator(child,cache).value(child_start,source_start,count);
}
#endif

struct Batch {
    std::vector<std::vector<Fr>> points;
    std::vector<Fr> lambda,public_lambda,checked_lambda,tail;
    Fr tail_lambda=0,claim=0;
};
std::vector<std::vector<Fr>> childPoints(const MappedInputSpec &s,Fs &t) {
    std::vector<std::vector<Fr>> points;
    for (std::size_t j=0;j<s.children.size();++j)
        points.push_back(point(t,log2exact(s.children[j].commitment.spec.padded),
            "mapped.child.point/"+std::to_string(j)));
    return points;
}
Batch batch(const MappedInputSpec &s,const MappedInputProof &p,
    const std::vector<std::vector<Fr>> &points,Fs &t,const std::vector<CheckedInputClaim> &checked) {
    Batch b;b.points=points;
    // All child evaluations have been authenticated before these coefficients.
    for (std::size_t j=0;j<s.children.size();++j) {
        auto lambda=t.challenge("mapped.lambda/"+std::to_string(j));b.lambda.push_back(lambda);
        const auto &c=s.children[j];
        Fr bias=0;std::size_t i=0;
        for (const auto &span:c.spans) {
            bias+=(prefixWeight(points[j],i+span.count)-prefixWeight(points[j],i))*span.bias;
            i+=span.count;
        }
        b.claim+=lambda*(p.children[j].value-bias);
    }
    for (std::size_t j=0;j<s.public_values.size();++j) {
        auto lambda=t.challenge("mapped.public.lambda/"+std::to_string(j));
        b.public_lambda.push_back(lambda);b.claim+=lambda*s.public_values[j].value;
    }
    for(std::size_t j=0;j<checked.size();++j){
        auto lambda=t.challenge("mapped.checked.lambda/"+std::to_string(j));
        b.checked_lambda.push_back(lambda);b.claim+=lambda*checked[j].value;
    }
    b.tail=point(t,log2exact(s.master.spec.padded),"mapped.master.tail");
    b.tail_lambda=t.challenge("mapped.tail.lambda");
    return b;
}
#ifndef R1_VERIFY_ONLY
std::vector<Fr> kernel(const MappedInputSpec &s,const Batch &b,const std::vector<CheckedInputClaim> &checked) {
    auto k=weights(b.tail);
    for (std::size_t i=0;i<k.size();++i)
        k[i]=i<s.master.spec.actual ? Fr(0) : k[i]*b.tail_lambda;
    for (std::size_t j=0;j<s.children.size();++j) {
        auto w=weights(b.points[j]);std::size_t i=0;
        for (const auto &span:s.children[j].spans)
            for (std::size_t z=0;z<span.count;++z) k[span.offset+z]+=b.lambda[j]*w[i++];
    }
    for (std::size_t j=0;j<s.public_values.size();++j)
        k[s.public_values[j].index]+=b.public_lambda[j];
    for(std::size_t j=0;j<checked.size();++j){
        const auto w=weights(checked[j].point);std::size_t i=0;
        for(const auto &span:checked[j].spans)for(std::size_t z=0;z<span.count;++z)k[span.offset+z]+=b.checked_lambda[j]*w[i++];
    }
    return k;
}
#endif
Fr kernelValueImpl(const MappedInputSpec &s,const Batch &b,const std::vector<Fr> &r,const std::vector<CheckedInputClaim> &checked) {
    Fr out=0;
    const EqWeights master(r); // One immutable point snapshot for this invocation.
    for (std::size_t j=0;j<s.children.size();++j) {
        const SpanEvaluator evaluator(b.points[j],master);
        std::size_t i=0;Fr v=0;
        for (const auto &span:s.children[j].spans) {
            v+=evaluator.value(i,span.offset,span.count);
            i+=span.count;
        }
        out+=b.lambda[j]*v;
    }
    for (std::size_t j=0;j<s.public_values.size();++j)
        out+=b.public_lambda[j]*master.at(s.public_values[j].index);
    for(std::size_t j=0;j<checked.size();++j){
        const SpanEvaluator evaluator(checked[j].point,master);
        std::size_t i=0;Fr v=0;for(const auto &span:checked[j].spans){v+=evaluator.value(i,span.offset,span.count);i+=span.count;}
        out+=b.checked_lambda[j]*v;
    }
    // Product of two eq polynomials: sum on tail, not eq at one point.
    Fr total=1;
    // Avoid division, including vanishing normalization factors: digit DP.
    Fr equal_prefix=1,below=0;
    std::vector<Fr> suffix(r.size()+1,Fr(1));
    for (std::size_t bit=0;bit<r.size();++bit)
        suffix[bit+1]=suffix[bit]*((Fr(1)-r[bit])*(Fr(1)-b.tail[bit])+r[bit]*b.tail[bit]);
    total=suffix.back();
    for (unsigned bit=r.size();bit-->0;) {
        const Fr zero=(Fr(1)-r[bit])*(Fr(1)-b.tail[bit]),one=r[bit]*b.tail[bit];
        if ((s.master.spec.actual>>bit)&1) {
            below+=equal_prefix*zero*suffix[bit];equal_prefix*=one;
        } else equal_prefix*=zero;
    }
    const Fr tail=s.master.spec.actual==s.master.spec.padded ? Fr(0) : total-below;
    out+=b.tail_lambda*tail;
    return out;
}
Fr kernelValue(const MappedInputSpec &s,const Batch &b,const std::vector<Fr> &r,const std::vector<CheckedInputClaim> &checked) {
 p0b_diag_hits={};++p0b_diag_calls;const auto start=std::chrono::steady_clock::now();const auto cpu=std::clock();
 const Fr value=kernelValueImpl(s,b,r,checked);
 const double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(),cpu_s=double(std::clock()-cpu)/CLOCKS_PER_SEC;
 std::cerr<<std::setprecision(12)<<"P0B_REAL_KERNEL mode=p0b calls="<<p0b_diag_calls<<" wall_s="<<wall<<" cpu_s="<<cpu_s<<" singleton_hits="<<p0b_diag_hits.singleton<<" contiguous_hits="<<p0b_diag_hits.contiguous<<" fallback_hits="<<p0b_diag_hits.fallback<<" cache_peak_bytes="<<p0b_diag_hits.cache_peak_bytes<<" result_hex=";
 for(auto byte:encode(value))std::cerr<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(byte);
 std::cerr<<std::dec<<std::setfill(' ')<<'\n';return value;
}
} // namespace
#ifndef R1_VERIFY_ONLY
std::vector<Fr> gatherInput(const MappedInputSpec &s,std::size_t j,const std::vector<Fr> &w) {
    s.validate();require(w.size()==s.master.spec.padded && j<s.children.size(),"gather shape");
    const auto &c=s.children[j];std::vector<Fr> v(c.commitment.spec.padded,Fr(0));
    std::size_t i=0;
    for (const auto &span:c.spans)
        for (std::size_t k=0;k<span.count;++k) v[i++]=w[span.offset+k]+span.bias;
    return v;
}
MappedInputProof proveMappedInput(const MappedInputSpec &s,const std::vector<Fr> &w,
    const std::vector<std::vector<Fr>> &children,Fs &t,const std::vector<CheckedInputClaim> &checked) {
    appendMappedInput(s,t);appendCheckedInput(s,checked,t);require(w.size()==s.master.spec.padded && children.size()==s.children.size(),
        "mapped witness shape");
    MappedInputProof p;auto points=childPoints(s,t);
    for (std::size_t j=0;j<children.size();++j) {
        // Validate the identical affine gather and zero tail without allocating
        // a second full child (in particular, the padded full-model W).
        const auto &child=children[j];const auto &mapping=s.children[j];
        require(child.size()==mapping.commitment.spec.padded,"mapped witness child mismatch/padding");
        const std::string mismatch="mapped witness child mismatch/padding";
        std::size_t at=0;
        for(const auto &span:mapping.spans)
            for(std::size_t z=0;z<span.count;++z)
                require(child[at++]==w[span.offset+z]+span.bias,mismatch);
        for(;at<child.size();++at)
            require(child[at].isZero(),mismatch);
        const auto &c=s.children[j].commitment;
        p.children.push_back(open(c.spec,c,children[j],points[j],t,"mapped.child/"+std::to_string(j)));
    }
    auto b=batch(s,p,points,t,checked);auto k=kernel(s,b,checked);
    require(dot(k,w)==b.claim,"mapped witness public values/master padding");
    // An initializer_list owns const vectors and copies both factors again.
    // Transfer the kernel and retain only the one immutable-witness working
    // copy required by the existing in-place Sumcheck folds. The factor order,
    // polynomial messages and transcript are unchanged.
    std::vector<std::vector<Fr>> factors;factors.reserve(2);
    factors.emplace_back(std::move(k));factors.emplace_back(w);
    auto end=proveSumcheck(std::move(factors),2,[](const auto &v){return v[0]*v[1];},p.sumcheck,t,"mapped.input");
    p.master=open(s.master.spec,s.master,w,end.point,t,"mapped.master");
    return p;
}
#endif
void verifyMappedInput(const MappedInputSpec &s,const MappedInputProof &p,Fs &t,const std::vector<CheckedInputClaim> &checked) {
    appendMappedInput(s,t);appendCheckedInput(s,checked,t);require(p.children.size()==s.children.size(),"mapped proof child count");
    auto points=childPoints(s,t);
    for (std::size_t j=0;j<s.children.size();++j) {
        const auto &c=s.children[j].commitment;
        verify(c.spec,c,points[j],p.children[j],t,"mapped.child/"+std::to_string(j));
    }
    auto b=batch(s,p,points,t,checked);
    auto end=verifySumcheck(log2exact(s.master.spec.padded),2,b.claim,p.sumcheck,t,"mapped.input");
    verify(s.master.spec,s.master,end.point,p.master,t,"mapped.master");
    require(end.value==kernelValue(s,b,end.point,checked)*p.master.value,"mapped input final kernel/opening");
}
} // namespace zkgpt::r1
