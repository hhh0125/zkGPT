#include "tests/zkgpt_artifact_test_fixture.hpp"

#include "generator_cache.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

enum class VerifyStage {
    accepted,
    format,
    protocol_version,
    statement_validation,
    artifact_binding,
    range_statement,
    range_proof,
    circuit_fingerprint,
    gkr_sumcheck,
    fconn_v,
    lasso,
    ipa_opening,
    public_output,
    other
};

const char *stageName(VerifyStage stage) {
    switch (stage) {
        case VerifyStage::accepted: return "ACCEPTED";
        case VerifyStage::format: return "Format/Deserialization";
        case VerifyStage::protocol_version: return "Protocol Version";
        case VerifyStage::statement_validation: return "Statement Validation";
        case VerifyStage::artifact_binding: return "Artifact Binding";
        case VerifyStage::range_statement: return "Range Statement/Layout";
        case VerifyStage::range_proof: return "Range Proof Verification";
        case VerifyStage::circuit_fingerprint: return "Circuit Fingerprint";
        case VerifyStage::gkr_sumcheck: return "GKR Sumcheck";
        case VerifyStage::fconn_v: return "FCONN-V Matrix Sumcheck";
        case VerifyStage::lasso: return "Lasso";
        case VerifyStage::ipa_opening: return "Hyrax/IPA";
        case VerifyStage::public_output: return "Public Output Claim";
        case VerifyStage::other: return "Other";
    }
    return "Other";
}

VerifyStage classifyVerifyError(const std::string &error) {
    if (error.find("unsupported zkGPT protocol version")!=std::string::npos)
        return VerifyStage::protocol_version;
    if (error.find("unsupported zkGPT circuit id")!=std::string::npos ||
        error.find("unsupported zkGPT model shape")!=std::string::npos ||
        error.find("top-level and Range model shapes")!=std::string::npos)
        return VerifyStage::statement_validation;
    if (error.find("GKR proof output does not match public output claim")!=
        std::string::npos ||
        error.find("GKR output evaluation does not match public output claim")!=
        std::string::npos)
        return VerifyStage::public_output;
    if (error.find("top-level transcript mismatch")!=std::string::npos)
        return VerifyStage::artifact_binding;
    if (error.find("Range verification failed")!=std::string::npos) {
        if (error.find("descriptor")!=std::string::npos ||
            error.find("generator count")!=std::string::npos ||
            error.find("commitment row count")!=std::string::npos ||
            error.find("public range query shape")!=std::string::npos ||
            error.find("range chunk widths")!=std::string::npos ||
            error.find("range region")!=std::string::npos ||
            error.find("query regions")!=std::string::npos)
            return VerifyStage::range_statement;
        if (error.find("IPA")!=std::string::npos ||
            error.find("opening failed")!=std::string::npos)
            return VerifyStage::ipa_opening;
        return VerifyStage::range_proof;
    }
    if (error.find("Range statement")!=std::string::npos &&
        error.find("public registry")!=std::string::npos)
        return VerifyStage::range_statement;
    if (error.find("GKR verification failed")!=std::string::npos) {
        if (error.find("fingerprint")!=std::string::npos)
            return VerifyStage::circuit_fingerprint;
        if (error.find("matrix v")!=std::string::npos)
            return VerifyStage::fconn_v;
        if (error.find("Lasso")!=std::string::npos)
            return VerifyStage::lasso;
        if (error.find("input commitment opening")!=std::string::npos ||
            error.find("IPA")!=std::string::npos)
            return VerifyStage::ipa_opening;
        if (error.find("phase-")!=std::string::npos ||
            error.find("sumcheck")!=std::string::npos ||
            error.find("wiring predicate")!=std::string::npos ||
            error.find("matrix u")!=std::string::npos)
            return VerifyStage::gkr_sumcheck;
        if (error.find("output")!=std::string::npos)
            return VerifyStage::public_output;
    }
    return VerifyStage::other;
}

bool canonicalRoundTrip(ZkGPTPublicStatement &statement,ZkGPTProof &proof,
                        std::string *error) {
    try {
        const auto statement_bytes=serializeZkGPTPublicStatement(statement);
        const auto proof_bytes=serializeZkGPTProof(proof);
        ZkGPTPublicStatement decoded_statement;
        ZkGPTProof decoded_proof;
        if (!deserializeZkGPTPublicStatement(
                statement_bytes,decoded_statement,error) ||
            !deserializeZkGPTProof(proof_bytes,decoded_proof,error))
            return false;
        if (serializeZkGPTPublicStatement(decoded_statement)!=statement_bytes ||
            serializeZkGPTProof(decoded_proof)!=proof_bytes) {
            if (error) *error="canonical round-trip changed bytes";
            return false;
        }
        statement=std::move(decoded_statement);
        proof=std::move(decoded_proof);
        return true;
    } catch (const std::exception &exception) {
        if (error) *error=exception.what();
        return false;
    }
}

struct SemanticCase {
    std::string name;
    std::string object;
    VerifyStage expected_without_binding;
    VerifyStage expected_with_binding;
    std::function<void(ZkGPTPublicStatement &,ZkGPTProof &)> mutate;
};

struct CaseResult {
    std::string name;
    std::string object;
    bool rebound=false;
    bool binding_available=true;
    bool round_trip=false;
    VerifyStage expected=VerifyStage::other;
    VerifyStage actual=VerifyStage::other;
    bool accepted=false;
    bool passed=false;
    std::string error;
};

CaseResult runSemanticCase(
    const SemanticCase &test,bool rebound,
    const ZkGPTPublicStatement &baseline_statement,
    const ZkGPTProof &baseline_proof) {
    ZkGPTPublicStatement statement=baseline_statement;
    ZkGPTProof proof=baseline_proof;
    test.mutate(statement,proof);
    CaseResult result;
    result.name=test.name;
    result.object=test.object;
    result.rebound=rebound;
    result.expected=rebound ? test.expected_with_binding
                            : test.expected_without_binding;
    if (rebound) {
        try {
            bindZkGPTProof(statement,proof);
        } catch (const std::exception &exception) {
            result.binding_available=false;
            result.error=exception.what();
        }
    }
    std::string round_trip_error;
    result.round_trip=canonicalRoundTrip(statement,proof,&round_trip_error);
    if (!result.round_trip && result.error.empty())
        result.error=round_trip_error;

    // A protocol-version mutation is intentionally rejected by both canonical
    // serialization and bindZkGPTProof.  Still call the unchanged production
    // verifier on the typed object to exercise its independent version check.
    std::string verify_error;
    result.accepted=verifyZkGPT(statement,proof,&verify_error);
    result.actual=result.accepted ? VerifyStage::accepted
                                  : classifyVerifyError(verify_error);
    if (!verify_error.empty()) result.error=verify_error;
    const bool expected_accept=result.expected==VerifyStage::accepted;
    const bool outcome_ok=result.accepted==expected_accept &&
                          result.actual==result.expected;
    const bool serialization_ok=result.round_trip ||
        result.expected==VerifyStage::protocol_version;
    const bool binding_ok=!rebound || result.binding_available ||
        result.expected==VerifyStage::protocol_version;
    result.passed=outcome_ok && serialization_ok && binding_ok;
    return result;
}

void preserveRoundSumMutation(GKRQuadraticRound &round) {
    round.a+=Fr(1);
    round.b-=Fr(1);
}

template <typename T>
T &firstNonEmpty(std::vector<T> &items,const char *what) {
    if (items.empty()) throw std::runtime_error(std::string("missing ")+what);
    return items.front();
}

std::size_t findBytes(const std::vector<std::uint8_t> &haystack,
                      const std::vector<std::uint8_t> &needle) {
    const auto found=std::search(
        haystack.begin(),haystack.end(),needle.begin(),needle.end());
    if (found==haystack.end())
        throw std::runtime_error("canonical field encoding not found");
    return static_cast<std::size_t>(found-haystack.begin());
}

void setU32(std::vector<std::uint8_t> &bytes,std::size_t offset,
            std::uint32_t value) {
    if (offset+4>bytes.size()) throw std::runtime_error("u32 offset overflow");
    for (unsigned i=0;i<4;++i)
        bytes[offset+i]=static_cast<std::uint8_t>(value>>(8*i));
}

struct FormatCase {
    std::string name;
    std::string object;
    std::function<void(std::vector<std::uint8_t> &,
                       std::vector<std::uint8_t> &)> mutate;
};

bool runFormatCase(const FormatCase &test,
                   const std::vector<std::uint8_t> &baseline_statement,
                   const std::vector<std::uint8_t> &baseline_proof,
                   std::string *actual_error) {
    auto statement_bytes=baseline_statement;
    auto proof_bytes=baseline_proof;
    test.mutate(statement_bytes,proof_bytes);
    ZkGPTPublicStatement statement;
    ZkGPTProof proof;
    std::string statement_error,proof_error;
    const bool statement_ok=deserializeZkGPTPublicStatement(
        statement_bytes,statement,&statement_error);
    const bool proof_ok=deserializeZkGPTProof(proof_bytes,proof,&proof_error);
    if (!statement_ok) *actual_error="statement: "+statement_error;
    else if (!proof_ok) *actual_error="proof: "+proof_error;
    else *actual_error="both artifacts decoded";
    return !statement_ok || !proof_ok;
}

}  // namespace

int main(int argc,char **argv) {
    initPairing(mcl::BN254);
    try {
        std::string output_prefix;
        if (argc==3 && std::string(argv[1])=="--artifact-prefix")
            output_prefix=argv[2];
        else if (argc!=1)
            throw std::invalid_argument(
                "usage: zkgpt_artifact_mutation_tests "
                "[--artifact-prefix PATH]");

        gkr_test::SmallZkGPTArtifactFixture fixture;
        const auto &baseline_statement=fixture.statement();
        const auto &baseline_proof=fixture.proof();
        if (!output_prefix.empty()) fixture.writeArtifact(output_prefix);
        std::cout << "zkGPT artifact mutation baseline accepted: statement_bytes="
                  << fixture.statementBytes().size() << ", proof_bytes="
                  << fixture.proofBytes().size() << '\n';

        std::size_t parent_index=baseline_proof.gkr_proof.layers.size();
        std::size_t fconn_index=baseline_proof.gkr_proof.layers.size();
        for (std::size_t i=0;i<baseline_proof.gkr_proof.layers.size();++i) {
            const auto &layer=baseline_proof.gkr_proof.layers[i];
            if (parent_index==baseline_proof.gkr_proof.layers.size() &&
                !layer.phase1_rounds.empty())
                parent_index=i;
            if (fconn_index==baseline_proof.gkr_proof.layers.size() &&
                !layer.matrix_v_rounds.empty())
                fconn_index=i;
        }
        if (parent_index>=baseline_proof.gkr_proof.layers.size() ||
            fconn_index>=baseline_proof.gkr_proof.layers.size())
            throw std::runtime_error("artifact fixture lacks GKR mutation fields");
        if (baseline_proof.gkr_proof.lasso.rounds.empty() ||
            baseline_proof.gkr_proof.lasso.input_opening.ipa.rounds.empty())
            throw std::runtime_error("artifact fixture lacks Lasso/IPA fields");
        if (baseline_proof.range_proof.membership_proofs.empty() ||
            baseline_proof.range_proof.membership_proofs[0].empty() ||
            baseline_proof.range_proof.membership_proofs[0][0]
                .reciprocal_value_sumcheck.rounds.empty())
            throw std::runtime_error("artifact fixture lacks Range proof fields");

        const auto &main_generators=getGeneratorSet(
            baseline_statement.range_statement.val0_generator_domain);
        std::vector<SemanticCase> cases;
        cases.push_back({
            "protocol_version", "statement.protocol_version",
            VerifyStage::protocol_version,VerifyStage::protocol_version,
            [](ZkGPTPublicStatement &statement,ZkGPTProof &) {
                statement.protocol_version=1;
            }});
        cases.push_back({
            "circuit_fingerprint", "statement.circuit_fingerprint[0]",
            VerifyStage::artifact_binding,VerifyStage::circuit_fingerprint,
            [](ZkGPTPublicStatement &statement,ZkGPTProof &) {
                statement.circuit_fingerprint[0]^=1;
            }});
        cases.push_back({
            "model_shape_top_only", "statement.model_shape.mlp_dimension",
            VerifyStage::statement_validation,VerifyStage::statement_validation,
            [](ZkGPTPublicStatement &statement,ZkGPTProof &) {
                ++statement.model_shape.mlp_dimension;
            }});
        cases.push_back({
            "model_shape_coordinated",
            "statement.model_shape + range_statement.shape.mlp_dimension",
            VerifyStage::artifact_binding,VerifyStage::ipa_opening,
            [](ZkGPTPublicStatement &statement,ZkGPTProof &) {
                ++statement.model_shape.mlp_dimension;
                ++statement.range_statement.shape.mlp_dimension;
            }});
        cases.push_back({
            "val0_commitment", "range_statement.val0_commitment[0]",
            VerifyStage::artifact_binding,VerifyStage::ipa_opening,
            [&](ZkGPTPublicStatement &statement,ZkGPTProof &) {
                statement.range_statement.val0_commitment[0]+=
                    main_generators.generators[0];
            }});
        cases.push_back({
            "input_generator_domain",
            "range_statement.val0_generator_domain.domain",
            VerifyStage::artifact_binding,VerifyStage::range_statement,
            [](ZkGPTPublicStatement &statement,ZkGPTProof &) {
                statement.range_statement.val0_generator_domain.domain=
                    "zkGPT/range";
            }});
        cases.push_back({
            "input_generator_count",
            "range_statement.val0_generator_domain.count",
            VerifyStage::artifact_binding,VerifyStage::range_statement,
            [](ZkGPTPublicStatement &statement,ZkGPTProof &) {
                ++statement.range_statement.val0_generator_domain.count;
            }});
        cases.push_back({
            "gkr_output_only", "proof.gkr_proof.output_evaluation",
            VerifyStage::public_output,VerifyStage::public_output,
            [](ZkGPTPublicStatement &,ZkGPTProof &proof) {
                proof.gkr_proof.output_evaluation+=Fr(1);
            }});
        cases.push_back({
            "public_output_coordinated",
            "output_claim + gkr_proof.output_evaluation",
            VerifyStage::artifact_binding,VerifyStage::gkr_sumcheck,
            [](ZkGPTPublicStatement &statement,ZkGPTProof &proof) {
                statement.output_claim.mle_evaluation+=Fr(1);
                proof.gkr_proof.output_evaluation+=Fr(1);
            }});
        cases.push_back({
            "gkr_phase1_polynomial",
            "gkr_proof.layers[parent].phase1_rounds[0].a/b",
            VerifyStage::artifact_binding,VerifyStage::gkr_sumcheck,
            [=](ZkGPTPublicStatement &,ZkGPTProof &proof) {
                preserveRoundSumMutation(
                    proof.gkr_proof.layers[parent_index].phase1_rounds[0]);
            }});
        cases.push_back({
            "gkr_fconn_v_polynomial",
            "gkr_proof.layers[fconn].matrix_v_rounds[0].a/b",
            VerifyStage::artifact_binding,VerifyStage::fconn_v,
            [=](ZkGPTPublicStatement &,ZkGPTProof &proof) {
                preserveRoundSumMutation(
                    proof.gkr_proof.layers[fconn_index].matrix_v_rounds[0]);
            }});
        cases.push_back({
            "gkr_lasso_claim", "gkr_proof.lasso.mapping_evaluation",
            VerifyStage::artifact_binding,VerifyStage::lasso,
            [](ZkGPTPublicStatement &,ZkGPTProof &proof) {
                proof.gkr_proof.lasso.mapping_evaluation+=Fr(1);
            }});
        cases.push_back({
            "gkr_ipa_left", "gkr_proof.lasso.input_opening.ipa.rounds[0].left",
            VerifyStage::artifact_binding,VerifyStage::ipa_opening,
            [&](ZkGPTPublicStatement &,ZkGPTProof &proof) {
                proof.gkr_proof.lasso.input_opening.ipa.rounds[0].left+=
                    main_generators.generators[0];
            }});
        cases.push_back({
            "range_layout_name", "range_statement.regions[0].name",
            VerifyStage::artifact_binding,VerifyStage::ipa_opening,
            [](ZkGPTPublicStatement &statement,ZkGPTProof &) {
                firstNonEmpty(statement.range_statement.regions,"Range region")
                    .name+=".mutated";
            }});
        cases.push_back({
            "range_query_bits", "range_statement.queries[0].bits",
            VerifyStage::artifact_binding,VerifyStage::range_statement,
            [](ZkGPTPublicStatement &statement,ZkGPTProof &) {
                ++firstNonEmpty(
                    statement.range_statement.queries,"Range query").bits;
            }});
        cases.push_back({
            "range_sumcheck_polynomial",
            "range_proof.membership[0][0].reciprocal_value_sumcheck.rounds[0].value_at_2",
            VerifyStage::artifact_binding,VerifyStage::range_proof,
            [](ZkGPTPublicStatement &,ZkGPTProof &proof) {
                proof.range_proof.membership_proofs[0][0]
                    .reciprocal_value_sumcheck.rounds[0].value_at_2+=Fr(1);
            }});
        cases.push_back({
            "shared_val0_log_size", "range_statement.val0_log_size",
            VerifyStage::artifact_binding,VerifyStage::range_statement,
            [](ZkGPTPublicStatement &statement,ZkGPTProof &) {
                ++statement.range_statement.val0_log_size;
            }});
        cases.push_back({
            "artifact_binding", "proof.transcript_binding",
            VerifyStage::artifact_binding,VerifyStage::accepted,
            [](ZkGPTPublicStatement &,ZkGPTProof &proof) {
                proof.transcript_binding+=Fr(1);
            }});

        std::vector<CaseResult> semantic_results;
        std::size_t semantic_passed=0;
        std::cout << "Mutation | binding | round-trip | expected | actual | "
                     "accepted | result\n";
        for (const auto &test : cases) {
            for (const bool rebound : {false,true}) {
                const auto result=runSemanticCase(
                    test,rebound,baseline_statement,baseline_proof);
                std::cout << result.name << " | "
                          << (rebound ? (result.binding_available
                                  ? "recomputed" : "unavailable")
                                      : "stale")
                          << " | " << (result.round_trip ? "yes" : "no")
                          << " | " << stageName(result.expected)
                          << " | " << stageName(result.actual)
                          << " | " << (result.accepted ? "yes" : "no")
                          << " | " << (result.passed ? "PASS" : "FAIL")
                          << '\n';
                std::cout << "  field: " << result.object << '\n'
                          << "  verifier: " << result.error << '\n';
                if (result.passed) ++semantic_passed;
                semantic_results.push_back(result);
            }
        }

        std::vector<std::uint8_t> first_commitment(
            G1::getSerializedByteSize());
        if (baseline_statement.range_statement.val0_commitment[0].serialize(
                first_commitment.data(),first_commitment.size())!=
            first_commitment.size())
            throw std::runtime_error("cannot encode baseline G1");
        const std::size_t commitment_offset=findBytes(
            fixture.statementBytes(),first_commitment);
        std::vector<FormatCase> format_cases;
        format_cases.push_back({"zkgpt_statement_v1","ZKGS version",
            [](auto &statement,auto &) { setU32(statement,4,1); }});
        format_cases.push_back({"zkgpt_proof_v1","ZKPF version",
            [](auto &,auto &proof) { setU32(proof,4,1); }});
        format_cases.push_back({"nested_gkr_v1","nested ZKGP version",
            [](auto &,auto &proof) { setU32(proof,16,1); }});
        format_cases.push_back({"nested_gkr_v2","nested ZKGP version",
            [](auto &,auto &proof) { setU32(proof,16,2); }});
        format_cases.push_back({"statement_trailing","ZKGS trailing byte",
            [](auto &statement,auto &) { statement.push_back(0); }});
        format_cases.push_back({"proof_trailing","ZKPF trailing byte",
            [](auto &,auto &proof) { proof.push_back(0); }});
        format_cases.push_back({"statement_truncated","ZKGS truncation",
            [](auto &statement,auto &) { statement.pop_back(); }});
        format_cases.push_back({"proof_truncated","ZKPF truncation",
            [](auto &,auto &proof) { proof.pop_back(); }});
        format_cases.push_back({"proof_invalid_nested_length",
            "ZKPF nested GKR length",
            [](auto &,auto &proof) { setU32(proof,8,0x7fffffffU); }});
        format_cases.push_back({"noncanonical_top_fr",
            "ZKPF transcript_binding Fr",
            [](auto &,auto &proof) {
                std::fill(proof.end()-32,proof.end(),0xff);
            }});
        format_cases.push_back({"noncanonical_nested_g1",
            "nested Range val0_commitment[0] G1",
            [=](auto &statement,auto &) {
                std::fill(statement.begin()+commitment_offset,
                          statement.begin()+commitment_offset+
                              first_commitment.size(),0xff);
            }});

        std::size_t format_passed=0;
        std::cout << "Encoding mutation | object | actual | result\n";
        for (const auto &test : format_cases) {
            std::string error;
            const bool passed=runFormatCase(
                test,fixture.statementBytes(),fixture.proofBytes(),&error);
            std::cout << test.name << " | " << test.object
                      << " | " << error << " | "
                      << (passed ? "PASS" : "FAIL") << '\n';
            if (passed) ++format_passed;
        }

        std::cout << "zkGPT semantic mutation tests: " << semantic_passed
                  << '/' << semantic_results.size() << " passed\n";
        std::cout << "zkGPT encoding mutation tests: " << format_passed
                  << '/' << format_cases.size() << " passed\n";
        if (semantic_passed!=semantic_results.size() ||
            format_passed!=format_cases.size())
            return 1;
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "zkGPT artifact mutation test failure: "
                  << exception.what() << '\n';
        return 1;
    }
}
