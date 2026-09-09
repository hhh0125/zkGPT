#pragma once

#include "prover.hpp"
#include "hyrax.hpp"
#include "gkr_protocol.hpp"
#include "range_protocol.hpp"

using namespace hyrax;
using std::unique_ptr;

// Prover-side Fiat-Shamir transcript driver.
//
// This class owns no verification trust boundary: it has direct access to the
// witness-bearing prover and records the resulting transcript as a GKRProof.
// The proof must subsequently be checked by the witness-free ::verifyGKR(...)
// function in gkr_verifier.hpp.
class GKRProofBuilder
{
public:
    prover *p;
    const layeredCircuit &C;

    GKRProofBuilder(prover *pr, const layeredCircuit &cir);

    void build();
    const GKRProof &proof() const { return gkr_proof_; }
    void setRangeProverTime(double value) { range_prover_time = value; }

    timer total_timer, total_slow_timer;
    double builder_check_time;
    double prover_time;
    double range_prover_time = 0;
    double matrix_time;
    Commit_return comm;

private:
    vector<vector<F>> r_u, r_v;
    vector<F> final_claim_u0, final_claim_v0;
    bool buildLayerProofs();
    bool buildLassoProof();
    bool buildInputOpeningProof();

    vector<F> beta_g;
    void betaInitPhase1(u32 depth, const F &alpha, const F &beta,
                        const vector<F>::const_iterator &r_0,
                        const vector<F>::const_iterator &r_1,
                        const F &relu_rou);
    void betaInitPhase2(u32 depth);

    F uni_value[2];
    F bin_value[3];
    void predicatePhase1(u32 layer_id);
    void predicatePhase2(u32 layer_id);

    F getFinalValue(const F &claim_u0, const F &claim_u1,
                    const F &claim_v0, const F &claim_v1);

    F eval_in;
    Transcript transcript_;
    GKRProof gkr_proof_;

    F challenge(const std::string &label);
    void appendQuadratic(const std::string &label,
                         const quadratic_poly &poly);
};
