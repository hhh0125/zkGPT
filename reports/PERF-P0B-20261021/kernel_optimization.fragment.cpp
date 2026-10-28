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

