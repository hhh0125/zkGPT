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

    GKRProofBuilder(prover *pr, const layeredCircuit &cir,
                    const WitnessShape &model_shape,
                    const GeneratorDomain &generator_domain,
                    ChallengeObserver *observer = nullptr,
                    const std::string &zk_context = std::string());

    void build();
    const GKRProof &proof() const {
        if (zkMode()) throw std::logic_error("use typed ZK proof getter in ZK mode");
        return gkr_proof_;
    }
    const GKRPublicStatement &publicStatement() const {
        if (zkMode()) throw std::logic_error("use typed ZK statement getter in ZK mode");
        return public_statement_;
    }
    bool zkMode() const {return !zk_context_.empty();}
    const GKRZkProof& zkProof() const {
        if (!zkMode()) throw std::logic_error("builder is in legacy mode");return zk_proof_;
    }
    const GKRZkPublicStatement& zkPublicStatement() const {
        if (!zkMode()) throw std::logic_error("builder is in legacy mode");return zk_statement_;
    }
    const zkgpt::zk::Parameters& zkParameters() const {return zk_parameters_;}
    const std::vector<Fr>& zkRowBlinders() const {return zk_row_blinders_;}
    std::array<std::uint8_t,32> transcriptStateDigest() const {
        return transcript_.stateDigest();
    }
    std::uint64_t transcriptChallengeCounter() const {
        return transcript_.challengeCounter();
    }
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
    // Input/weight evaluation points and claims produced by the second
    // (parent-v-point) FCONN matrix sumcheck.  These are folded into Lasso so
    // the final values are authenticated by the val[0] commitment opening.
    vector<vector<F>> fconn_v_r_u, fconn_v_r_v;
    vector<F> fconn_v_final_u0, fconn_v_final_v0;
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
    GKRPublicStatement public_statement_;
    std::string zk_context_;
    zkgpt::zk::Parameters zk_parameters_;
    GKRZkPublicStatement zk_statement_;
    GKRZkProof zk_proof_;
    std::vector<Fr> zk_row_blinders_;
    std::vector<zkgpt::zk::ScalarWitness> zk_u0_,zk_v0_,zk_matrix_v_u0_,zk_matrix_v_v0_;
    zkgpt::zk::ScalarWitness zk_u1_,zk_v1_,zk_previous_,zk_input_;
    std::unique_ptr<zkgpt::zk::MaskedSumcheckProver> zk_normal_mask_;

    F challenge(const std::string &label);
    void appendQuadratic(const std::string &label,
                         const quadratic_poly &poly);
    F roundChallenge(const std::string& label,const quadratic_poly& poly);
};
