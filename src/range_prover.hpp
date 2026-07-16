#ifndef ZKCNN_RANGE_PROVER_HPP
#define ZKCNN_RANGE_PROVER_HPP

#include "global_var.hpp"
#include "circuit.h"
#include "polynomial.h"
#include "hyrax_rp.hpp"
#include "range_protocol.hpp"
#include "range_wide.hpp"
#include "witness_registry.hpp"
using std::unique_ptr;
const int MAXL=24;  

class neuralNetwork;
enum class NonlinearOpType {
    Attention, LayerNorm, GELU, NonLinear
};

inline int next_power_of_two_exp(int n) {
    if (n <= 1) return 0;
    return 32 - __builtin_clz(n - 1);
}

class Constraint {
public:
    std::size_t actual_query_size = 0;
    int query_size = 0;
    unsigned range_size = 0;
    vector<range_wide::EncodedWitnessValue> inputs;
    vector<range_wide::RangeChunkQuery> chunk_queries;
};

class OP {
public:
    NonlinearOpType op_type;
    vector<Constraint> constraints;
};

struct RangeQueryRegion {
    WitnessKind kind;
    string name;
    std::size_t val0_offset;
    std::size_t count;
    unsigned bits;
    bool is_signed;
    std::size_t proof_constraint_index;
    std::size_t proof_start;
};

class range_prover {
public:
    // Constructor
    range_prover(int layer_num = 0, int head_num = 0, int head_dim = 0, 
                int attn_dim = 0, int linear_dim = 0, int seq_len = 0, 
                int threads = 32, int merged = 0) 
        : LayerNum(layer_num), HeadNum(head_num), HeadDim(head_dim),
          AttnDim(attn_dim), LinearDim(linear_dim), seq_len(seq_len), 
          thread_num(threads), merged(merged) {}

    struct SC_Return {
        Fr* random;
        Fr claim_f;
        Fr claim_g;
    };
    int merged = 0;
    SC_Return sumcheck_deg1(int l, Fr* f, Fr S);
    SC_Return sumcheck_deg3(int l, Fr* r, Fr* f, Fr* g, Fr S);
    int LayerNum, HeadNum, HeadDim, AttnDim, LinearDim, seq_len;// llm related parameters
    int thread_num = 32;
    vector<OP> ops;
    G1 g[1<<(MAXL/2)],GG;
    Fr r[MAXL];
    void init();
    void push_back(NonlinearOpType op_type, const std::vector<std::pair<int, int>>& constraint_params);
    timer prove_timer;
    timer prepare_timer;
    vector<vector<G1>> range_prove(const Constraint &constraint, int thread_num);
    vector<G1> logup(ll * f,ll * t,int m,int n,int thread_num);
    double prove();
    RangeProof proveStageB(const RangePublicStatement &statement);
    RangePublicStatement makePublicStatement(
        int val0_log_size, const G1 *val0_commitments,
        std::size_t commitment_count, const G1 *val0_generators,
        std::size_t generator_count, const G1 &val0_u) const;
    void build();
    void buildFromWitness(const vector<F> &val0,
                          const WitnessRegistry &registry);
    void verifyWitnessConsistency() const;
    const vector<RangeQueryRegion> &queryRegions() const { return query_regions; }
    void tamperBuiltValueForTest(std::size_t region_index = 0);
private:
    const vector<F> *witness_source = nullptr;
    vector<RangeQueryRegion> query_regions;
    WitnessShape witness_shape;
    bool built_from_witness = false;

    static range_wide::EncodedWitnessValue encodeWitnessValue(
        const F &field_value, const RangeConstraint &constraint,
        std::size_t absolute_offset);
    static void buildChunkQueries(Constraint &constraint);
    double proveMembership(
        vector<vector<vector<G1>>> *chunk_commitments);
    ReconstructionProof proveReconstruction(
        std::size_t query_index, const Constraint &constraint,
        Transcript &transcript, vector<Fr> *point) const;
    void validateShape(const WitnessShape &shape);
};
__int128 convert(Fr x)	;

#endif //ZKCNN_RANGE_PROVER_HPP
