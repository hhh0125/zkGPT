#include "gkr_verifier.hpp"

#include "generator_cache.hpp"
#include "hyrax_opening.hpp"
#include "polynomial.h"
#include "utils.hpp"
#include <cybozu/sha2.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <thread>
#include <sstream>

namespace {

using std::size_t;
using std::string;
using std::vector;

class FingerprintWriter {
public:
    ~FingerprintWriter() { flush(); }
    void u64(std::uint64_t value) {
        for (unsigned i=0;i<8;++i)
            buffer_.push_back(static_cast<std::uint8_t>(value>>(8*i)));
        if (buffer_.size()>=kFlushSize) flush();
    }
    void text(const char *value) {
        const string text_value(value);
        u64(text_value.size());
        buffer_.insert(buffer_.end(),text_value.begin(),text_value.end());
    }
    std::array<std::uint8_t,32> finish() {
        flush();
        std::array<std::uint8_t,32> result{};
        if (hash_.digest(result.data(),result.size(),nullptr,0)!=result.size())
            throw std::runtime_error("failed to finalize circuit fingerprint");
        finished_=true;
        return result;
    }
private:
    static constexpr size_t kFlushSize=1ULL<<20;
    cybozu::Sha256 hash_;
    vector<std::uint8_t> buffer_;
    bool finished_=false;
    void flush() {
        if (!finished_ && !buffer_.empty()) {
            hash_.update(buffer_.data(),buffer_.size());
            buffer_.clear();
        }
    }
};

void fail(const string &message) {
    throw std::runtime_error(message);
}

void appendQuadratic(Transcript &transcript, const string &label,
                     const GKRQuadraticRound &round) {
    transcript.appendFr(label + ".a", round.a);
    transcript.appendFr(label + ".b", round.b);
    transcript.appendFr(label + ".c", round.c);
}

Fr evaluate(const GKRQuadraticRound &round, const Fr &point) {
    return round.a * point * point + round.b * point + round.c;
}

Transcript initializeTranscript(const layeredCircuit &circuit,
                               const GKRPublicStatement &statement) {
    Transcript transcript("zkGPT-proof-v1");
    transcript.appendU64("protocol.version", statement.protocol_version);
    transcript.appendString("curve.id", "BN254");
    transcript.appendU64("circuit.layer_count", circuit.size);
    for (const auto &layer : circuit.circuit) {
        transcript.appendU64("circuit.layer.type",
            static_cast<std::uint64_t>(layer.ty));
        transcript.appendU64("circuit.layer.size", layer.size);
        transcript.appendU64("circuit.layer.bit_length", layer.bit_length);
        transcript.appendU64("circuit.layer.max_bl_u", layer.max_bl_u);
        transcript.appendU64("circuit.layer.max_bl_v", layer.max_bl_v);
        transcript.appendBool("circuit.layer.need_phase2", layer.need_phase2);
        transcript.appendU64("circuit.layer.zero_start", layer.zero_start_id);
    }
    transcript.appendU64("input.log_size", statement.val0_log_size);
    transcript.appendU64("input.commitment_count",
                         statement.val0_commitment.size());
    for (const auto &commitment : statement.val0_commitment)
        transcript.appendG1("input.commitment", commitment);
    return transcript;
}

void validateStatement(const layeredCircuit &circuit,
                       const GKRPublicStatement &statement) {
    if (statement.protocol_version != 1)
        fail("GKR statement has unsupported protocol version");
    if (circuit.size < 2 || circuit.circuit.size() != circuit.size)
        fail("GKR public circuit has invalid layer count");
    if (statement.val0_log_size < 0 || statement.val0_log_size > 30)
        fail("GKR statement has invalid val0 log size");
    const size_t expected_commitments = size_t{1}
        << (statement.val0_log_size / 2);
    if (statement.val0_commitment.size() != expected_commitments)
        fail("GKR statement commitment count does not match val0 shape");
    const size_t expected_generators = size_t{1}
        << (statement.val0_log_size - statement.val0_log_size / 2);
    if (statement.val0_generator_domain.domain != "zkGPT/main" ||
        statement.val0_generator_domain.count != expected_generators)
        fail("GKR statement has invalid val0 generator descriptor");
    if (fingerprintCircuit(circuit)!=statement.circuit_fingerprint)
        fail("GKR public circuit fingerprint mismatch");
    const auto &shape = statement.model_shape;
    if (shape.sequence_length == 0 || shape.layer_count == 0 ||
        shape.head_count == 0 || shape.head_dimension == 0 ||
        shape.hidden_dimension == 0 || shape.mlp_dimension == 0 ||
        shape.head_count > std::numeric_limits<size_t>::max() /
                           shape.head_dimension ||
        shape.head_count * shape.head_dimension != shape.hidden_dimension)
        fail("GKR statement has invalid model shape");
}

void computePredicates(const layeredCircuit &circuit, std::uint32_t layer_id,
                       const vector<Fr> &r_g0, const vector<Fr> &r_g1,
                       const Fr &alpha, const Fr &beta,
                       const Fr &zero_scale, const vector<Fr> &r_u,
                       const vector<Fr> *r_v, Fr uni[2], Fr bin[3]) {
    const auto &current = circuit.circuit.at(layer_id);
    vector<Fr> beta_g(size_t{1} << current.bit_length);
    initBetaTable(beta_g, current.bit_length, r_g0.begin(), r_g1.begin(),
                  alpha, beta);
    if (current.zero_start_id < current.size) {
        for (size_t gate = current.zero_start_id; gate < beta_g.size(); ++gate)
            beta_g[gate] *= zero_scale;
    }

    vector<Fr> beta_u(size_t{1} << current.max_bl_u);
    initBetaTable(beta_u, current.max_bl_u, r_u.begin(), F_ONE);
    uni[0].clear();
    uni[1].clear();
    bin[0].clear();
    bin[1].clear();
    bin[2].clear();
    const size_t unary_threads = current.uni_gates.size() < 16384 ? 1 :
        std::min<size_t>(32, std::thread::hardware_concurrency() == 0
                                ? 32 : std::thread::hardware_concurrency());
    vector<std::array<Fr, 2>> unary_partial(unary_threads);
    vector<std::thread> workers;
    workers.reserve(unary_threads);
    for (size_t thread_id = 0; thread_id < unary_threads; ++thread_id) {
        workers.emplace_back([&, thread_id] {
            const size_t begin = current.uni_gates.size() * thread_id /
                                 unary_threads;
            const size_t end = current.uni_gates.size() * (thread_id + 1) /
                               unary_threads;
            for (size_t i = begin; i < end; ++i) {
                const auto &gate = current.uni_gates[i];
                if (gate.g >= beta_g.size() || gate.u >= beta_u.size())
                    continue;
                const unsigned source_class = gate.lu == 0 ? 0U : 1U;
                unary_partial[thread_id][source_class] +=
                    beta_g[gate.g] * beta_u[gate.u] * gate.sc;
            }
        });
    }
    for (auto &worker : workers) worker.join();
    for (const auto &partial : unary_partial) {
        uni[0] += partial[0];
        uni[1] += partial[1];
    }
    for (const auto &gate : current.uni_gates)
        if (gate.g >= beta_g.size() || gate.u >= beta_u.size())
            fail("GKR public circuit contains an out-of-range unary gate");

    if (!r_v) return;
    vector<Fr> beta_v(size_t{1} << current.max_bl_v);
    initBetaTable(beta_v, current.max_bl_v, r_v->begin(), F_ONE);
    uni[0] *= beta_v[0];
    uni[1] *= beta_v[0];
    const size_t binary_threads = current.bin_gates.size() < 16384 ? 1 :
        std::min<size_t>(32, std::thread::hardware_concurrency() == 0
                                ? 32 : std::thread::hardware_concurrency());
    vector<std::array<Fr, 3>> binary_partial(binary_threads);
    workers.clear();
    workers.reserve(binary_threads);
    for (size_t thread_id = 0; thread_id < binary_threads; ++thread_id) {
        workers.emplace_back([&, thread_id] {
            const size_t begin = current.bin_gates.size() * thread_id /
                                 binary_threads;
            const size_t end = current.bin_gates.size() * (thread_id + 1) /
                               binary_threads;
            for (size_t i = begin; i < end; ++i) {
                const auto &gate = current.bin_gates[i];
                if (gate.g >= beta_g.size() || gate.u >= beta_u.size() ||
                    gate.v >= beta_v.size() || gate.l > 2)
                    continue;
                binary_partial[thread_id][gate.l] +=
                    beta_g[gate.g] * beta_u[gate.u] * beta_v[gate.v] * gate.sc;
            }
        });
    }
    for (auto &worker : workers) worker.join();
    for (const auto &partial : binary_partial)
        for (unsigned i = 0; i < 3; ++i) bin[i] += partial[i];
    for (const auto &gate : current.bin_gates)
        if (gate.g >= beta_g.size() || gate.u >= beta_u.size() ||
            gate.v >= beta_v.size() || gate.l > 2)
            fail("GKR public circuit contains an out-of-range binary gate");
}

Fr finalPredicateValue(const Fr uni[2], const Fr bin[3],
                       const GKRLayerProof &layer) {
    return bin[0] * layer.final_claim_u0 * layer.final_claim_v0 +
           bin[1] * layer.final_claim_u1 * layer.final_claim_v1 +
           bin[2] * layer.final_claim_u1 * layer.final_claim_v0 +
           uni[0] * layer.final_claim_u0 +
           uni[1] * layer.final_claim_u1;
}

} // namespace

std::array<std::uint8_t, 32> fingerprintCircuit(
    const layeredCircuit &circuit) {
    FingerprintWriter writer;
    writer.text("zkGPT-circuit-v1");
    writer.u64(circuit.size);
    writer.u64(circuit.circuit.size());
    for (const auto &layer : circuit.circuit) {
        writer.u64(static_cast<std::uint64_t>(layer.ty));
        writer.u64(layer.size);
        writer.u64(layer.size_u[0]);
        writer.u64(layer.size_u[1]);
        writer.u64(layer.size_v[0]);
        writer.u64(layer.size_v[1]);
        writer.u64(static_cast<std::uint8_t>(layer.bit_length_u[0]));
        writer.u64(static_cast<std::uint8_t>(layer.bit_length_u[1]));
        writer.u64(static_cast<std::uint8_t>(layer.bit_length_v[0]));
        writer.u64(static_cast<std::uint8_t>(layer.bit_length_v[1]));
        writer.u64(static_cast<std::uint8_t>(layer.bit_length));
        writer.u64(static_cast<std::uint8_t>(layer.max_bl_u));
        writer.u64(static_cast<std::uint8_t>(layer.max_bl_v));
        writer.u64(layer.need_phase2 ? 1 : 0);
        writer.u64(layer.zero_start_id);
        writer.u64(static_cast<std::uint8_t>(layer.fft_bit_length));
        writer.u64(layer.uni_interval.size());
        for (const auto &interval : layer.uni_interval) {
            writer.u64(static_cast<std::uint64_t>(interval.first));
            writer.u64(static_cast<std::uint64_t>(interval.second));
        }
        writer.u64(layer.bin_interval.size());
        for (const auto &interval : layer.bin_interval) {
            writer.u64(static_cast<std::uint64_t>(interval.first));
            writer.u64(static_cast<std::uint64_t>(interval.second));
        }
        writer.u64(layer.uni_gates.size());
        for (const auto &gate : layer.uni_gates) {
            writer.u64(gate.g);
            writer.u64(gate.u);
            writer.u64(gate.lu);
            writer.u64(static_cast<std::uint64_t>(gate.sc));
        }
        writer.u64(layer.bin_gates.size());
        for (const auto &gate : layer.bin_gates) {
            writer.u64(gate.g);
            writer.u64(gate.u);
            writer.u64(gate.v);
            writer.u64(gate.l);
            writer.u64(static_cast<std::uint64_t>(gate.sc));
        }
        writer.u64(layer.ori_id_u.size());
        for (const auto value : layer.ori_id_u) writer.u64(value);
        writer.u64(layer.ori_id_v.size());
        for (const auto value : layer.ori_id_v) writer.u64(value);
    }
    return writer.finish();
}

std::string circuitFingerprintHex(
    const std::array<std::uint8_t, 32> &fingerprint) {
    static const char digits[]="0123456789abcdef";
    string result;
    result.reserve(fingerprint.size()*2);
    for (const auto byte : fingerprint) {
        result.push_back(digits[byte>>4]);
        result.push_back(digits[byte&15]);
    }
    return result;
}

bool verifyGKR(const layeredCircuit &circuit,
               const GKRPublicStatement &statement,
               const GKRProof &proof, std::string *error) {
    try {
        validateStatement(circuit, statement);
        Transcript transcript = initializeTranscript(circuit, statement);
        const auto &output_layer = circuit.circuit.back();
        vector<vector<Fr>> r_u(circuit.size + 2), r_v(circuit.size + 2);
        r_u[circuit.size].resize(output_layer.bit_length);
        for (auto &coordinate : r_u[circuit.size])
            coordinate = transcript.challenge("gkr.output.point");

        if (proof.output_evaluation != statement.output_evaluation)
            fail("GKR output evaluation does not match public output claim");
        Fr previous_sum = proof.output_evaluation;
        transcript.appendFr("gkr.output.evaluation", previous_sum);
        if (proof.layers.size() != circuit.size - 1)
            fail("GKR proof layer count does not match public circuit");

        vector<Fr> final_u0(circuit.size + 2), final_v0(circuit.size + 2);
        Fr final_u1 = 0;
        Fr final_v1 = 0;
        Fr alpha = F_ONE;
        Fr beta = F_ZERO;
        vector<Fr> r_g0 = r_u[circuit.size];
        vector<Fr> r_g1;
        const unsigned sequence_bits =
            ceilPow2BitLength(statement.model_shape.sequence_length);

        for (size_t proof_index = 0; proof_index < proof.layers.size();
             ++proof_index) {
            const std::uint32_t layer_id =
                circuit.size - 1 - static_cast<std::uint32_t>(proof_index);
            const auto &current = circuit.circuit.at(layer_id);
            const auto &layer = proof.layers[proof_index];
            if (layer.layer_index != layer_id ||
                layer.layer_type != static_cast<std::uint32_t>(current.ty))
                fail("GKR proof layer metadata mismatch at layer " +
                     std::to_string(layer_id));

            const Fr zero_scale = current.zero_start_id < current.size
                ? transcript.challenge("gkr.relu.zero_scale") : F_ONE;

            if (current.ty == layerType::FCONN) {
                if (!layer.phase1_rounds.empty() ||
                    !layer.phase2_rounds.empty())
                    fail("GKR matrix layer contains normal sumcheck rounds");
                const size_t m = layer.matrix_rounds.size();
                if (m > 30 || current.bit_length < sequence_bits)
                    fail("GKR matrix layer has invalid dimensions");
                const size_t k = current.bit_length - sequence_bits;
                if (r_g0.size() < sequence_bits + k)
                    fail("GKR matrix layer output point is too short at layer " +
                         std::to_string(layer_id));
                r_u[layer_id].resize(m + sequence_bits);
                r_v[layer_id].resize(m + k);
                for (size_t j = 0; j < sequence_bits; ++j)
                    r_u[layer_id][m + j] = r_g0[k + j];
                for (size_t j = 0; j < k; ++j)
                    r_v[layer_id][m + j] = r_g0[j];

                Fr matrix_claim = final_u1;
                const string label = "gkr.matrix." +
                                     std::to_string(layer_id);
                for (size_t round_index = 0; round_index < m;
                     ++round_index) {
                    const auto &round = layer.matrix_rounds[round_index];
                    if (evaluate(round, F_ZERO) + evaluate(round, F_ONE) !=
                        matrix_claim)
                        fail("GKR matrix sumcheck claim mismatch at layer " +
                             std::to_string(layer_id));
                    appendQuadratic(transcript, label, round);
                    const Fr challenge = transcript.challenge(label +
                                                               ".challenge");
                    r_u[layer_id][round_index] = challenge;
                    r_v[layer_id][round_index] = challenge;
                    matrix_claim = evaluate(round, challenge);
                }
                if (layer.final_claim_u0 * layer.final_claim_v0 != matrix_claim)
                    fail("GKR matrix final product mismatch at layer " +
                         std::to_string(layer_id));
                if (!layer.final_claim_u1.isZero() ||
                    !layer.final_claim_v1.isZero())
                    fail("GKR matrix layer has nonzero secondary claim");
                transcript.appendFr(label + ".final_f",
                                    layer.final_claim_u0);
                transcript.appendFr(label + ".final_g",
                                    layer.final_claim_v0);
            } else {
                if (!layer.matrix_rounds.empty())
                    fail("GKR normal layer contains matrix sumcheck rounds");
                if (layer.phase1_rounds.size() !=
                    static_cast<size_t>(current.max_bl_u))
                    fail("GKR phase-1 round count mismatch at layer " +
                         std::to_string(layer_id));
                r_u[layer_id].resize(current.max_bl_u);
                for (size_t round_index = 0;
                     round_index < layer.phase1_rounds.size(); ++round_index) {
                    const auto &round = layer.phase1_rounds[round_index];
                    if (evaluate(round, F_ZERO) + evaluate(round, F_ONE) !=
                        previous_sum)
                        fail("GKR phase-1 claim mismatch at layer " +
                             std::to_string(layer_id));
                    appendQuadratic(transcript, "gkr.phase1", round);
                    const Fr challenge =
                        transcript.challenge("gkr.phase1.challenge");
                    r_u[layer_id][round_index] = challenge;
                    previous_sum = evaluate(round, challenge);
                }

                if (current.need_phase2) {
                    if (layer.phase2_rounds.size() !=
                        static_cast<size_t>(current.max_bl_v))
                        fail("GKR phase-2 round count mismatch at layer " +
                             std::to_string(layer_id));
                    r_v[layer_id].resize(current.max_bl_v);
                    for (size_t round_index = 0;
                         round_index < layer.phase2_rounds.size();
                         ++round_index) {
                        const auto &round = layer.phase2_rounds[round_index];
                        if (evaluate(round, F_ZERO) + evaluate(round, F_ONE) !=
                            previous_sum)
                            fail("GKR phase-2 claim mismatch at layer " +
                                 std::to_string(layer_id));
                        appendQuadratic(transcript, "gkr.phase2", round);
                        const Fr challenge =
                            transcript.challenge("gkr.phase2.challenge");
                        r_v[layer_id][round_index] = challenge;
                        previous_sum = evaluate(round, challenge);
                    }
                } else {
                    if (!layer.phase2_rounds.empty() ||
                        !layer.final_claim_v1.isZero())
                        fail("GKR one-phase layer contains phase-2 data");
                }

                Fr uni[2];
                Fr bin[3];
                computePredicates(circuit, layer_id, r_g0, r_g1,
                                  alpha, beta, zero_scale, r_u[layer_id],
                                  current.need_phase2 ? &r_v[layer_id] : nullptr,
                                  uni, bin);
                if (finalPredicateValue(uni, bin, layer) != previous_sum)
                    fail("GKR final wiring predicate mismatch at layer " +
                         std::to_string(layer_id));
            }

            final_u0[layer_id] = layer.final_claim_u0;
            final_u1 = layer.final_claim_u1;
            final_v0[layer_id] = layer.final_claim_v0;
            final_v1 = layer.final_claim_v1;
            transcript.appendFr("gkr.layer.final_u0", layer.final_claim_u0);
            transcript.appendFr("gkr.layer.final_u1", layer.final_claim_u1);
            transcript.appendFr("gkr.layer.final_v0", layer.final_claim_v0);
            transcript.appendFr("gkr.layer.final_v1", layer.final_claim_v1);
            alpha = current.bit_length_u[1] >= 0
                ? transcript.challenge("gkr.layer.alpha") : F_ZERO;
            beta = (current.bit_length_v[1] >= 0 ||
                    current.ty == layerType::FFT)
                ? transcript.challenge("gkr.layer.beta") : F_ZERO;
            previous_sum = alpha * final_u1 + beta * final_v1;
            r_g0 = r_u[layer_id];
            r_g1 = r_v[layer_id];
        }

        vector<Fr> sigma_u(circuit.size - 1);
        vector<Fr> sigma_v(circuit.size - 1);
        for (auto &value : sigma_u)
            value = transcript.challenge("lasso.sigma_u");
        for (auto &value : sigma_v)
            value = transcript.challenge("lasso.sigma_v");
        previous_sum.clear();
        for (std::uint32_t layer_id = 1; layer_id < circuit.size;
             ++layer_id) {
            const auto &layer = circuit.circuit[layer_id];
            if (layer.bit_length_u[0] >= 0)
                previous_sum += sigma_u[layer_id - 1] * final_u0[layer_id];
            if (layer.bit_length_v[0] >= 0)
                previous_sum += sigma_v[layer_id - 1] * final_v0[layer_id];
        }

        const auto input_bits = circuit.circuit[0].bit_length;
        if (proof.lasso.rounds.size() != static_cast<size_t>(input_bits))
            fail("GKR Lasso round count does not match input layer");
        r_u[0].resize(input_bits);
        for (size_t round_index = 0;
             round_index < proof.lasso.rounds.size(); ++round_index) {
            const auto &round = proof.lasso.rounds[round_index];
            if (evaluate(round, F_ZERO) + evaluate(round, F_ONE) !=
                previous_sum)
                fail("GKR Lasso sumcheck claim mismatch at round " +
                     std::to_string(round_index));
            appendQuadratic(transcript, "lasso.sumcheck", round);
            const Fr challenge =
                transcript.challenge("lasso.sumcheck.challenge");
            r_u[0][round_index] = challenge;
            previous_sum = evaluate(round, challenge);
        }
        transcript.appendFr("lasso.input_evaluation",
                            proof.lasso.input_evaluation);

        vector<Fr> beta_g(size_t{1} << input_bits);
        initBetaTable(beta_g, input_bits, r_u[0].begin(), F_ONE);
        Fr mapping_evaluation = F_ZERO;
        for (std::uint32_t layer_id = 1; layer_id < circuit.size;
             ++layer_id) {
            const auto &layer = circuit.circuit[layer_id];
            if (layer.bit_length_u[0] >= 0) {
                vector<Fr> weights(size_t{1} << layer.bit_length_u[0]);
                initBetaTable(weights, layer.bit_length_u[0],
                              r_u[layer_id].begin(), sigma_u[layer_id - 1]);
                for (size_t j = 0; j < layer.size_u[0]; ++j) {
                    if (j >= layer.ori_id_u.size() ||
                        layer.ori_id_u[j] >= beta_g.size())
                        fail("GKR public circuit has invalid Lasso u mapping");
                    mapping_evaluation += beta_g[layer.ori_id_u[j]] * weights[j];
                }
            }
            if (layer.bit_length_v[0] >= 0) {
                vector<Fr> weights(size_t{1} << layer.bit_length_v[0]);
                initBetaTable(weights, layer.bit_length_v[0],
                              r_v[layer_id].begin(), sigma_v[layer_id - 1]);
                for (size_t j = 0; j < layer.size_v[0]; ++j) {
                    if (j >= layer.ori_id_v.size() ||
                        layer.ori_id_v[j] >= beta_g.size())
                        fail("GKR public circuit has invalid Lasso v mapping");
                    mapping_evaluation += beta_g[layer.ori_id_v[j]] * weights[j];
                }
            }
        }
        if (proof.lasso.mapping_evaluation != mapping_evaluation)
            fail("GKR Lasso mapping evaluation mismatch");
        if (proof.lasso.input_evaluation * mapping_evaluation != previous_sum)
            fail("GKR Lasso final input claim mismatch");
        transcript.appendFr("lasso.mapping_evaluation", mapping_evaluation);

        const auto &generators =
            getGeneratorSet(statement.val0_generator_domain);
        string opening_error;
        if (!hyraxMleOpenVerifyRowMajor(
                statement.val0_commitment, r_u[0], generators.generators,
                generators.u, proof.lasso.input_evaluation,
                proof.lasso.input_opening, transcript, "gkr.input.opening",
                &opening_error))
            fail("GKR input commitment opening failed: " + opening_error);
        if (transcript.challenge("gkr.proof.final") !=
            proof.transcript_binding)
            fail("GKR final transcript binding mismatch");
        return true;
    } catch (const std::exception &exception) {
        if (error) *error = exception.what();
        return false;
    }
}
