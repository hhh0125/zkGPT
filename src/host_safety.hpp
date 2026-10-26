#pragma once
#include "global_var.hpp"
#include <cstdint>
#include <limits>
#include <cmath>
#include <stdexcept>

namespace host_safety {
inline __int128 add(__int128 a,__int128 b){__int128 r;if(__builtin_add_overflow(a,b,&r))throw std::overflow_error("signed128 addition overflow");return r;}
inline __int128 sub(__int128 a,__int128 b){__int128 r;if(__builtin_sub_overflow(a,b,&r))throw std::overflow_error("signed128 subtraction overflow");return r;}
inline __int128 mul(__int128 a,__int128 b){__int128 r;if(__builtin_mul_overflow(a,b,&r))throw std::overflow_error("signed128 multiplication overflow");return r;}
inline __int128 neg(__int128 a){return sub(0,a);}
inline __int128 divide(__int128 a,__int128 b){
    const __int128 lo=-(__int128(1)<<126)-(__int128(1)<<126);
    if(!b || (a==lo && b==-1))throw std::range_error("invalid signed128 division");return a/b;
}
inline __int128 power2(int bit){if(bit<0 || bit>126)throw std::range_error("invalid signed128 shift");return __int128(1)<<bit;}
// Preserve the floating input precision and std::round/truncation policy.
// Compare with exact powers of two before conversion: Float(MAX128) can round
// to 2^127, so it cannot be used as an inclusive upper bound.
template<class Float> inline __int128 truncated(Float v){
    const long double boundary=std::ldexp(1.0L,127);
    if(!std::isfinite(v) || static_cast<long double>(v)<-boundary || static_cast<long double>(v)>=boundary)
        throw std::range_error("floating value outside signed128 conversion domain");
    return static_cast<__int128>(v);
}
template<class Float> inline __int128 rounded(Float v){return truncated(std::round(v));}
inline int checkedSize(std::size_t size,__int128 extra=0){
    auto n=add(static_cast<__int128>(size),extra);
    if(n<0 || n>std::numeric_limits<int>::max())throw std::length_error("witness index exceeds int domain");return static_cast<int>(n);
}
inline __uint128_t magnitude(__int128 v) {
    return v<0 ? __uint128_t(-(v+1))+1 : __uint128_t(v);
}
// MCL's numeric constructor accepts only int64_t. Every signed128 integer is
// smaller than half this build's BN254 scalar modulus; byte encoding preserves
// its integer embedding, including MIN128, with no signed narrowing.
inline Fr field(__int128 v) {
    if(v>=std::numeric_limits<std::int64_t>::min() && v<=std::numeric_limits<std::int64_t>::max())
        return Fr(static_cast<std::int64_t>(v));
    auto u=magnitude(v);unsigned char bytes[16];
    for(unsigned i=0;i<16;++i){bytes[i]=static_cast<unsigned char>(u);u>>=8;}
    Fr f;f.setLittleEndianMod(bytes,sizeof(bytes));return v<0?-f:f;
}
}
