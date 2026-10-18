#include "tests/gkr_test_fixture.hpp"

#include "generator_cache.hpp"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

enum class RejectStage {
    accepted,
    format,
    output,
    phase1,
    phase2,
    parent_predicate,
    matrix_u,
    matrix_v,
    lasso_sumcheck,
    lasso_claim,
    ipa_opening,
    final_binding,
    other
};

const char *stageName(RejectStage stage) {
    switch (stage) {
        case RejectStage::accepted: return "ACCEPTED";
        case RejectStage::format: return "format";
        case RejectStage::output: return "output claim";
        case RejectStage::phase1: return "phase-1 sumcheck";
        case RejectStage::phase2: return "phase-2 sumcheck";
        case RejectStage::parent_predicate: return "parent wiring predicate";
        case RejectStage::matrix_u: return "FCONN-U matrix sumcheck";
        case RejectStage::matrix_v: return "FCONN-V matrix sumcheck";
        case RejectStage::lasso_sumcheck: return "Lasso sumcheck";
        case RejectStage::lasso_claim: return "Lasso aggregation claim";
        case RejectStage::ipa_opening: return "Hyrax/IPA opening";
        case RejectStage::final_binding: return "GKR final binding";
        case RejectStage::other: return "other";
    }
    return "other";
}

RejectStage classifyError(const std::string &error) {
    if (error.find("output evaluation")!=std::string::npos)
        return RejectStage::output;
    if (error.find("phase-1")!=std::string::npos)
        return RejectStage::phase1;
    if (error.find("phase-2")!=std::string::npos)
        return RejectStage::phase2;
    if (error.find("final wiring predicate")!=std::string::npos)
        return RejectStage::parent_predicate;
    if (error.find("matrix u")!=std::string::npos)
        return RejectStage::matrix_u;
    if (error.find("matrix v")!=std::string::npos)
        return RejectStage::matrix_v;
    if (error.find("Lasso sumcheck")!=std::string::npos)
        return RejectStage::lasso_sumcheck;
    if (error.find("Lasso mapping evaluation")!=std::string::npos ||
        error.find("Lasso final input claim")!=std::string::npos)
        return RejectStage::lasso_claim;
    if (error.find("input commitment opening")!=std::string::npos ||
        error.find("IPA")!=std::string::npos)
        return RejectStage::ipa_opening;
    if (error.find("final transcript binding")!=std::string::npos)
        return RejectStage::final_binding;
    if (error.find("deserial")!=std::string::npos ||
        error.find("serial")!=std::string::npos ||
        error.find("encoding")!=std::string::npos ||
        error.find("truncated")!=std::string::npos)
        return RejectStage::format;
    return RejectStage::other;
}

bool sameEntry(const ChallengeTraceEntry &a, const ChallengeTraceEntry &b) {
    return a.transcript_domain==b.transcript_domain &&
           a.transcript_instance==b.transcript_instance &&
           a.label==b.label && a.counter==b.counter &&
           a.pre_state_digest==b.pre_state_digest &&
           a.challenge_encoding==b.challenge_encoding &&
           a.post_state_digest==b.post_state_digest;
}

std::string firstTraceFork(
    const std::vector<ChallengeTraceEntry> &baseline,
    const std::vector<ChallengeTraceEntry> &mutated) {
    const std::size_t common=std::min(baseline.size(),mutated.size());
    std::size_t index=0;
    while (index<common && sameEntry(baseline[index],mutated[index])) ++index;
    if (index==common) {
        if (baseline.size()==mutated.size()) return "none (trace identical)";
        return "none (rejected before next challenge; matched prefix="+
            std::to_string(index)+")";
    }
    const auto &expected=baseline[index];
    const auto &actual=mutated[index];
    std::ostringstream output;
    output << actual.label << " counter=" << actual.counter
           << " pre-digest-diff="
           << (actual.pre_state_digest!=expected.pre_state_digest ? "yes" : "no")
           << " challenge-diff="
           << (actual.challenge_encoding!=expected.challenge_encoding
                   ? "yes" : "no");
    if (actual.label!=expected.label || actual.counter!=expected.counter)
        output << " baseline=" << expected.label
               << "@" << expected.counter;
    return output.str();
}

void preserveRoundSumMutation(GKRQuadraticRound &round) {
    round.a+=Fr(1);
    round.b-=Fr(1);
}

struct MutationCase {
    std::string name;
    std::string field;
    RejectStage expected_stage;
    std::function<void(GKRProof &)> mutate;
};

}  // namespace

int main() {
    initPairing(mcl::BN254);
    try {
        gkr_test::SmallFconnFixture fixture;
        const auto &baseline=fixture.proof();
        const auto &circuit=fixture.circuit();
        const auto &statement=fixture.statement();
        const std::size_t matrix_index=fixture.matrixLayerProofIndex();
        const std::size_t parent_index=fixture.parentLayerProofIndex();
        const auto &parent=baseline.layers.at(parent_index);
        const auto &matrix=baseline.layers.at(matrix_index);
        if (parent.phase1_rounds.size()<2 || parent.phase2_rounds.size()<2 ||
            matrix.matrix_rounds.size()<2 || matrix.matrix_v_rounds.size()<2 ||
            baseline.lasso.rounds.size()<2 ||
            baseline.lasso.input_opening.ipa.rounds.empty())
            throw std::runtime_error(
                "small GKR mutation fixture lacks required proof fields");

        std::string baseline_error;
        if (!verifyGKR(circuit,statement,baseline,&baseline_error))
            throw std::runtime_error(
                "mutation baseline was rejected: "+baseline_error);
        std::cout << "GKR mutation baseline accepted: proof_bytes="
                  << fixture.proofBytes().size()
                  << ", trace_entries="
                  << fixture.verifierTrace().entries.size() << '\n';

        const auto &generators=getGeneratorSet(statement.val0_generator_domain);
        std::vector<MutationCase> cases;
        cases.push_back({
            "phase1_polynomial", "layers[parent].phase1_rounds[0].a/b",
            RejectStage::phase1, [=](GKRProof &proof) {
                preserveRoundSumMutation(
                    proof.layers[parent_index].phase1_rounds[0]);
            }});
        cases.push_back({
            "phase2_polynomial", "layers[parent].phase2_rounds[0].a/b",
            RejectStage::phase2, [=](GKRProof &proof) {
                preserveRoundSumMutation(
                    proof.layers[parent_index].phase2_rounds[0]);
            }});
        cases.push_back({
            "normal_final_claim_u1", "layers[parent].final_claim_u1",
            RejectStage::parent_predicate, [=](GKRProof &proof) {
                proof.layers[parent_index].final_claim_u1+=Fr(1);
            }});
        cases.push_back({
            "normal_final_claim_v1", "layers[parent].final_claim_v1",
            RejectStage::parent_predicate, [=](GKRProof &proof) {
                proof.layers[parent_index].final_claim_v1+=Fr(1);
            }});
        cases.push_back({
            "fconn_u_polynomial", "layers[fconn].matrix_rounds[0].a/b",
            RejectStage::matrix_u, [=](GKRProof &proof) {
                preserveRoundSumMutation(
                    proof.layers[matrix_index].matrix_rounds[0]);
            }});
        cases.push_back({
            "fconn_v_polynomial", "layers[fconn].matrix_v_rounds[0].a/b",
            RejectStage::matrix_v, [=](GKRProof &proof) {
                preserveRoundSumMutation(
                    proof.layers[matrix_index].matrix_v_rounds[0]);
            }});
        cases.push_back({
            "fconn_u_final_evaluation", "layers[fconn].final_claim_u0",
            RejectStage::matrix_u, [=](GKRProof &proof) {
                proof.layers[matrix_index].final_claim_u0+=Fr(1);
            }});
        cases.push_back({
            "fconn_u_weight_evaluation", "layers[fconn].final_claim_v0",
            RejectStage::matrix_u, [=](GKRProof &proof) {
                proof.layers[matrix_index].final_claim_v0+=Fr(1);
            }});
        cases.push_back({
            "fconn_v_final_evaluation",
            "layers[fconn].matrix_v_final_claim_u0",
            RejectStage::matrix_v, [=](GKRProof &proof) {
                proof.layers[matrix_index].matrix_v_final_claim_u0+=Fr(1);
            }});
        cases.push_back({
            "fconn_v_weight_evaluation",
            "layers[fconn].matrix_v_final_claim_v0",
            RejectStage::matrix_v, [=](GKRProof &proof) {
                proof.layers[matrix_index].matrix_v_final_claim_v0+=Fr(1);
            }});
        cases.push_back({
            "lasso_mapping_claim", "lasso.mapping_evaluation",
            RejectStage::lasso_claim, [](GKRProof &proof) {
                proof.lasso.mapping_evaluation+=Fr(1);
            }});
        cases.push_back({
            "lasso_sumcheck_polynomial", "lasso.rounds[0].a/b",
            RejectStage::lasso_sumcheck, [](GKRProof &proof) {
                preserveRoundSumMutation(proof.lasso.rounds[0]);
            }});
        cases.push_back({
            "ipa_left_point", "lasso.input_opening.ipa.rounds[0].left",
            RejectStage::ipa_opening, [&](GKRProof &proof) {
                proof.lasso.input_opening.ipa.rounds[0].left+=
                    generators.generators[0];
            }});
        cases.push_back({
            "ipa_right_point", "lasso.input_opening.ipa.rounds[0].right",
            RejectStage::ipa_opening, [&](GKRProof &proof) {
                proof.lasso.input_opening.ipa.rounds[0].right+=
                    generators.generators[0];
            }});
        cases.push_back({
            "ipa_final_witness", "lasso.input_opening.ipa.final_witness",
            RejectStage::ipa_opening, [](GKRProof &proof) {
                proof.lasso.input_opening.ipa.final_witness+=Fr(1);
            }});
        cases.push_back({
            "output_evaluation", "proof.output_evaluation",
            RejectStage::output, [](GKRProof &proof) {
                proof.output_evaluation+=Fr(1);
            }});
        cases.push_back({
            "final_binding", "proof.transcript_binding",
            RejectStage::final_binding, [](GKRProof &proof) {
                proof.transcript_binding+=Fr(1);
            }});
        cases.push_back({
            "fconn_v_product_preserving_endpoints",
            "matrix_v_final_claim_u0 + matrix_v_final_claim_v0",
            RejectStage::matrix_u, [=](GKRProof &proof) {
                auto &layer=proof.layers[matrix_index];
                const Fr product=layer.matrix_v_final_claim_u0*
                                 layer.matrix_v_final_claim_v0;
                layer.matrix_v_final_claim_u0+=Fr(1);
                Fr inverse;
                Fr::inv(inverse,layer.matrix_v_final_claim_u0);
                layer.matrix_v_final_claim_v0=product*inverse;
            }});
        cases.push_back({
            "lasso_input_opening_claim", "lasso.input_evaluation",
            RejectStage::lasso_claim, [](GKRProof &proof) {
                proof.lasso.input_evaluation+=Fr(1);
            }});

        std::size_t passed=0;
        std::cout << "Mutation | field | expected | actual | trace fork | result\n";
        for (const auto &test : cases) {
            GKRProof mutated=baseline;
            test.mutate(mutated);

            GKRProof decoded;
            std::string error;
            bool format_ok=false;
            try {
                const auto bytes=serializeGKRProof(mutated);
                format_ok=deserializeGKRProof(bytes,decoded,&error);
            } catch (const std::exception &exception) {
                error=exception.what();
            }

            ChallengeTraceRecorder trace;
            bool accepted=false;
            RejectStage actual=RejectStage::format;
            if (format_ok) {
                error.clear();
                accepted=verifyGKR(circuit,statement,decoded,&error,&trace);
                actual=accepted ? RejectStage::accepted : classifyError(error);
            }
            const std::string fork=firstTraceFork(
                fixture.verifierTrace().entries,trace.entries);
            const bool pass=format_ok && !accepted &&
                            actual==test.expected_stage;
            std::cout << test.name << " | " << test.field << " | "
                      << stageName(test.expected_stage) << " | "
                      << stageName(actual) << " | " << fork << " | "
                      << (pass ? "PASS" : "FAIL") << '\n';
            std::cout << "  verifier: " << error << '\n';
            if (pass) ++passed;
        }

        std::cout << "GKR mutation tests: " << passed << '/' << cases.size()
                  << " passed\n";
        if (passed!=cases.size()) return 1;
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "GKR mutation test failure: " << exception.what() << '\n';
        return 1;
    }
}
