#pragma once

#include <cstddef>
#include <limits>
#include "paper_r1_mapped_input.hpp"
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

enum class WitnessKind {
    ROUNDING,
    LAYER_NORM,
    GELU,
    SOFTMAX
};

inline const char *witnessKindName(WitnessKind kind) {
    switch (kind) {
        case WitnessKind::ROUNDING: return "ROUNDING";
        case WitnessKind::LAYER_NORM: return "LAYER_NORM";
        case WitnessKind::GELU: return "GELU";
        case WitnessKind::SOFTMAX: return "SOFTMAX";
    }
    return "UNKNOWN";
}

struct WitnessShape {
    std::size_t sequence_length = 0;
    std::size_t layer_count = 0;
    std::size_t head_count = 0;
    std::size_t head_dimension = 0;
    std::size_t hidden_dimension = 0;
    std::size_t mlp_dimension = 0;
};

struct RangeConstraint {
    WitnessKind kind;
    std::string name;
    std::size_t val0_offset;
    std::size_t count;
    unsigned bits;
    bool is_signed;
};

inline bool fitsSignedBits(__int128 value, unsigned bits) {
    if (bits == 0 || bits > 126) return false;
    const __int128 half = static_cast<__int128>(1) << (bits - 1);
    return value >= -half && value < half;
}

inline bool fitsUnsignedBits(__int128 value, unsigned bits) {
    if (bits == 0 || bits > 126 || value < 0) return false;
    const unsigned __int128 limit =
        static_cast<unsigned __int128>(1) << bits;
    return static_cast<unsigned __int128>(value) < limit;
}

class WitnessRegistry {
public:
    void setR4NumericalCandidate(bool enabled) { r4_candidate_=enabled; }
    bool r4NumericalCandidate() const { return r4_candidate_; }
    void setR5BEngineeringCandidate(bool enabled) { r5b_candidate_=enabled; }
    void setR6Engineering(bool enabled) { r6_engineering_=enabled; }
    bool r6Engineering() const { return r6_engineering_; }
    bool r5bEngineeringCandidate() const { return r5b_candidate_; }
    void addExpQueries(std::size_t op,std::size_t to,std::size_t eo,std::size_t count) {
        // Paired spans preserve operator/query order without one entry per query.
        (void)op;
        append(exp_t_,to,count,Fr(0));
        append(exp_E_,eo,count,Fr(0));
    }
    const auto &rangeSources() const { return range_sources_; }
    const auto &expAddresses() const { return exp_t_; }
    const auto &expValues() const { return exp_E_; }
    const auto &parameterSources() const { return parameters_; }
    const auto &constants() const { return constants_; }
    const auto &reservedZero() const { return reserved_zero_; }
    std::size_t outputOffset() const { return output_offset_; }
    std::size_t outputCount() const { return output_count_; }
    void recordParameter(std::size_t offset,std::size_t count) {
        append(parameters_,offset,count,Fr(0));
    }
    void recordConstants(std::size_t offset,unsigned reserve) {
        constants_.push_back(offset);
        for(unsigned j=1;j<reserve;++j)reserved_zero_.push_back(offset+j);
    }
    void recordOutput(std::size_t offset,std::size_t count) {
        output_offset_=offset;output_count_=count;
    }
    void setShape(const WitnessShape &shape) {
        if (shape.sequence_length == 0 || shape.layer_count == 0 ||
            shape.head_count == 0 || shape.head_dimension == 0 ||
            shape.hidden_dimension == 0 || shape.mlp_dimension == 0) {
            throw std::invalid_argument("witness shape dimensions must be nonzero");
        }
        shape_ = shape;
    }

    const WitnessShape &shape() const { return shape_; }

    void addRange(WitnessKind kind, const std::string &name,
                  std::size_t val0_offset, std::size_t count,
                  unsigned bits, bool is_signed) {
        if (count == 0) return;
        if (name.empty())
            throw std::invalid_argument("range constraint name must not be empty");
        // The current Range Proof stores transformed values in signed __int128.
        // Keeping one headroom bit also avoids ambiguous field-to-integer casts.
        if (bits == 0 || bits > 126)
            throw std::invalid_argument("invalid range constraint bit width");
        if (val0_offset > std::numeric_limits<std::size_t>::max() - count)
            throw std::overflow_error("range constraint offset overflow: " + name);
        constraints_.push_back({kind, name, val0_offset, count, bits, is_signed});
        Fr bias=0;
        if(is_signed){bias=Fr(1);for(unsigned bit=1;bit<bits;++bit)bias*=Fr(2);}
        append(range_sources_[{bits,is_signed}],val0_offset,count,bias);
        if(r5b_candidate_ && kind==WitnessKind::LAYER_NORM && ends(name,"_sigma"))
            append(range_sources_[{bits,false}],val0_offset,count,Fr(-1));
        if(r4_candidate_){
            const bool ln=kind==WitnessKind::LAYER_NORM,gelu=kind==WitnessKind::GELU;
            const bool root=ln && (ends(name,"_sqrt_upper") || ends(name,"_sqrt_lower"));
            const bool positive=root || (ln && (ends(name,"_sigma") || ends(name,"_delta1") || ends(name,"_term1") || ends(name,"_term2"))) ||
                (gelu && (ends(name,"_delta2") || ends(name,"_term1") || ends(name,"_term2")));
            if(positive)append(range_sources_[{bits,false}],val0_offset,count,Fr(-1));
            if(ln && ends(name,"_sigma"))append(range_sources_[{32,false}],val0_offset,count,Fr(0));
        }
    }

    const std::vector<RangeConstraint> &constraints() const {
        return constraints_;
    }

    void clear() {
        constraints_.clear();
        range_sources_.clear();exp_t_.clear();exp_E_.clear();parameters_.clear();
        constants_.clear();reserved_zero_.clear();output_offset_=output_count_=0;
        r6_engineering_=false;
        shape_ = WitnessShape{};
        r4_candidate_ = false;
        r5b_candidate_ = false;
    }

private:
    bool r4_candidate_ = false;
    bool r6_engineering_=false;
    bool r5b_candidate_ = false;
    WitnessShape shape_;
    std::vector<RangeConstraint> constraints_;
    using Spans=std::vector<zkgpt::r1::InputSpan>;
    std::map<std::pair<unsigned,bool>,Spans> range_sources_;
    Spans exp_t_,exp_E_,parameters_;
    std::vector<std::size_t> constants_,reserved_zero_;
    std::size_t output_offset_=0,output_count_=0;
    static bool ends(const std::string&s,const std::string&suffix) {
        return s.size()>=suffix.size() && s.compare(s.size()-suffix.size(),suffix.size(),suffix)==0;
    }
    static void append(Spans&spans,std::size_t offset,std::size_t count,const Fr&bias) {
        if(!count)return;
        if(offset>std::numeric_limits<std::size_t>::max()-count)throw std::overflow_error("source offset overflow");
        if(!spans.empty() && spans.back().offset+spans.back().count==offset && spans.back().bias==bias)spans.back().count+=count;
        else spans.push_back({offset,count,bias});
    }
};
