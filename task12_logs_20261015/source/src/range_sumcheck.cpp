#include "range_sumcheck.hpp"

#include <stdexcept>

namespace {

bool failWith(std::string *error, const std::string &message) {
    if (error)
        *error = message;
    return false;
}

bool isPowerOfTwo(std::size_t value) { return value != 0 && (value & (value - 1)) == 0; }

std::size_t exactLog2(std::size_t value) {
    if (!isPowerOfTwo(value))
        throw std::invalid_argument("sumcheck vector size is not a power of two");
    std::size_t result = 0;
    while (value > 1) {
        value >>= 1;
        ++result;
    }
    return result;
}

Fr degree3Evaluate(const Degree3SumcheckRound &round, const Fr &point) {
    return Fr(1) / Fr(6) *
           ((-round.value_at_0) * (point - 1) * (point - 2) * (point - 3) +
            Fr(3) * round.value_at_1 * point * (point - 2) * (point - 3) -
            Fr(3) * round.value_at_2 * point * (point - 1) * (point - 3) +
            round.value_at_3 * point * (point - 1) * (point - 2));
}

std::vector<Fr> eqWeights(const std::vector<Fr> &point) {
    std::vector<Fr> weights(static_cast<std::size_t>(1) << point.size(), Fr(0));
    weights[0] = 1;
    std::size_t active = 1;
    for (const Fr &challenge : point) {
        for (std::size_t i = active; i > 0; --i) {
            const Fr previous = weights[i - 1];
            weights[i - 1] = previous * (Fr(1) - challenge);
            weights[i - 1 + active] = previous * challenge;
        }
        active *= 2;
    }
    return weights;
}

// Shared production kernels. Both legacy and masked modes use these exact
// computations and the same least-significant-coordinate folding order.
Degree1SumcheckRound degree1Round(const std::vector<Fr> &values) {
    Degree1SumcheckRound round;
    round.value_at_0 = 0;
    round.value_at_1 = 0;
    for (std::size_t i = 0; i < values.size(); i += 2) {
        round.value_at_0 += values[i];
        round.value_at_1 += values[i + 1];
    }
    return round;
}
Degree3SumcheckRound degree3Round(const std::vector<Fr> &equality, const std::vector<Fr> &f,
                                  const std::vector<Fr> &g) {
    Degree3SumcheckRound round;
    round.value_at_0 = round.value_at_1 = round.value_at_2 = round.value_at_3 = Fr(0);
    for (std::size_t i = 0; i < f.size(); i += 2) {
        const Fr e0 = equality[i], e1 = equality[i + 1], f0 = f[i], f1 = f[i + 1], g0 = g[i],
                 g1 = g[i + 1];
        round.value_at_0 += e0 * f0 * g0;
        round.value_at_1 += e1 * f1 * g1;
        round.value_at_2 += (Fr(2) * e1 - e0) * (Fr(2) * f1 - f0) * (Fr(2) * g1 - g0);
        round.value_at_3 +=
            (Fr(3) * e1 - Fr(2) * e0) * (Fr(3) * f1 - Fr(2) * f0) * (Fr(3) * g1 - Fr(2) * g0);
    }
    return round;
}
void foldValues(std::vector<Fr> &values, const Fr &challenge) {
    for (std::size_t i = 0; i < values.size() / 2; ++i)
        values[i] = values[2 * i] + challenge * (values[2 * i + 1] - values[2 * i]);
    values.resize(values.size() / 2);
}

void appendDegree1Round(Transcript &transcript, const std::string &label,
                        const Degree1SumcheckRound &round) {
    transcript.appendFr(label + ".value_at_0", round.value_at_0);
    transcript.appendFr(label + ".value_at_1", round.value_at_1);
}

void appendDegree3Round(Transcript &transcript, const std::string &label,
                        const Degree3SumcheckRound &round) {
    transcript.appendFr(label + ".value_at_0", round.value_at_0);
    transcript.appendFr(label + ".value_at_1", round.value_at_1);
    transcript.appendFr(label + ".value_at_2", round.value_at_2);
    transcript.appendFr(label + ".value_at_3", round.value_at_3);
}

Fr equalityEvaluation(const std::vector<Fr> &statement_point,
                      const std::vector<Fr> &opening_point) {
    if (statement_point.size() != opening_point.size())
        throw std::invalid_argument("equality point length mismatch");
    Fr result = 1;
    for (std::size_t i = 0; i < statement_point.size(); ++i)
        result *= statement_point[i] * opening_point[i] +
                  (Fr(1) - statement_point[i]) * (Fr(1) - opening_point[i]);
    return result;
}

} // namespace

Degree1SumcheckProof proveDegree1Sumcheck(const std::vector<Fr> &values_input,
                                          const Fr &initial_claim, Transcript &transcript,
                                          const std::string &label,
                                          std::vector<Fr> *opening_point) {
    exactLog2(values_input.size());
    Fr actual_claim = 0;
    for (const Fr &value : values_input)
        actual_claim += value;
    if (actual_claim != initial_claim)
        throw std::logic_error("degree-1 sumcheck witness does not match claim");

    std::vector<Fr> values = values_input;
    Degree1SumcheckProof proof;
    std::vector<Fr> point;
    Fr claim = initial_claim;
    while (values.size() > 1) {
        const auto round = degree1Round(values);
        if (round.value_at_0 + round.value_at_1 != claim)
            throw std::logic_error("degree-1 prover round consistency failed");
        proof.rounds.push_back(round);
        appendDegree1Round(transcript, label, round);
        const Fr challenge = transcript.challenge(label + ".challenge");
        point.push_back(challenge);
        claim = round.value_at_0 + challenge * (round.value_at_1 - round.value_at_0);
        foldValues(values, challenge);
    }
    proof.final_evaluation = values[0];
    if (proof.final_evaluation != claim)
        throw std::logic_error("degree-1 prover final claim failed");
    transcript.appendFr(label + ".final_evaluation", proof.final_evaluation);
    if (opening_point)
        *opening_point = std::move(point);
    return proof;
}

bool verifyDegree1Sumcheck(const Fr &initial_claim, const Degree1SumcheckProof &proof,
                           Transcript &transcript, const std::string &label,
                           std::vector<Fr> *opening_point, std::string *error) {
    try {
        Fr claim = initial_claim;
        std::vector<Fr> point;
        point.reserve(proof.rounds.size());
        for (const auto &round : proof.rounds) {
            appendDegree1Round(transcript, label, round);
            if (round.value_at_0 + round.value_at_1 != claim)
                return failWith(error, "degree-1 sumcheck round consistency failed");
            const Fr challenge = transcript.challenge(label + ".challenge");
            point.push_back(challenge);
            claim = round.value_at_0 + challenge * (round.value_at_1 - round.value_at_0);
        }
        transcript.appendFr(label + ".final_evaluation", proof.final_evaluation);
        if (claim != proof.final_evaluation)
            return failWith(error, "degree-1 sumcheck final claim failed");
        if (opening_point)
            *opening_point = std::move(point);
        return true;
    } catch (const std::exception &exception) {
        return failWith(error, exception.what());
    }
}

Degree3SumcheckProof proveDegree3Sumcheck(const std::vector<Fr> &equality_point,
                                          const std::vector<Fr> &f_input,
                                          const std::vector<Fr> &g_input, const Fr &initial_claim,
                                          Transcript &transcript, const std::string &label,
                                          std::vector<Fr> *opening_point) {
    if (f_input.size() != g_input.size() || equality_point.size() != exactLog2(f_input.size()))
        throw std::invalid_argument("invalid degree-3 sumcheck shape");
    std::vector<Fr> equality = eqWeights(equality_point);
    std::vector<Fr> f = f_input;
    std::vector<Fr> g = g_input;
    Fr actual_claim = 0;
    for (std::size_t i = 0; i < f.size(); ++i)
        actual_claim += equality[i] * f[i] * g[i];
    if (actual_claim != initial_claim)
        throw std::logic_error("degree-3 sumcheck witness does not match claim");

    Degree3SumcheckProof proof;
    std::vector<Fr> point;
    Fr claim = initial_claim;
    while (f.size() > 1) {
        const auto round = degree3Round(equality, f, g);
        if (round.value_at_0 + round.value_at_1 != claim)
            throw std::logic_error("degree-3 prover round consistency failed");
        proof.rounds.push_back(round);
        appendDegree3Round(transcript, label, round);
        const Fr challenge = transcript.challenge(label + ".challenge");
        point.push_back(challenge);
        claim = degree3Evaluate(round, challenge);
        foldValues(equality, challenge);
        foldValues(f, challenge);
        foldValues(g, challenge);
    }
    proof.final_f_evaluation = f[0];
    proof.final_g_evaluation = g[0];
    if (claim != equality[0] * f[0] * g[0])
        throw std::logic_error("degree-3 prover final claim failed");
    transcript.appendFr(label + ".final_f", proof.final_f_evaluation);
    transcript.appendFr(label + ".final_g", proof.final_g_evaluation);
    if (opening_point)
        *opening_point = std::move(point);
    return proof;
}

bool verifyDegree3Sumcheck(const std::vector<Fr> &equality_point, const Fr &initial_claim,
                           const Degree3SumcheckProof &proof, Transcript &transcript,
                           const std::string &label, std::vector<Fr> *opening_point,
                           std::string *error) {
    try {
        if (proof.rounds.size() != equality_point.size())
            return failWith(error, "degree-3 sumcheck round count mismatch");
        Fr claim = initial_claim;
        std::vector<Fr> point;
        point.reserve(proof.rounds.size());
        for (const auto &round : proof.rounds) {
            appendDegree3Round(transcript, label, round);
            if (round.value_at_0 + round.value_at_1 != claim)
                return failWith(error, "degree-3 sumcheck round consistency failed");
            const Fr challenge = transcript.challenge(label + ".challenge");
            point.push_back(challenge);
            claim = degree3Evaluate(round, challenge);
        }
        transcript.appendFr(label + ".final_f", proof.final_f_evaluation);
        transcript.appendFr(label + ".final_g", proof.final_g_evaluation);
        const Fr equality = equalityEvaluation(equality_point, point);
        if (claim != equality * proof.final_f_evaluation * proof.final_g_evaluation)
            return failWith(error, "degree-3 sumcheck final claim failed");
        if (opening_point)
            *opening_point = std::move(point);
        return true;
    } catch (const std::exception &exception) {
        return failWith(error, exception.what());
    }
}

namespace zkgpt::zk {
namespace {
constexpr std::uint64_t kMaskMagic = 0x315a43534d544b5aULL;
constexpr std::uint64_t kDegree1Magic = 0x315a314443544b5aULL;
constexpr std::uint64_t kDegree3Magic = 0x315a334443544b5aULL;
std::size_t coefficientCount(std::size_t variables, unsigned degree) {
    if (variables > 60 || degree < 1 || degree > 3)
        throw std::invalid_argument("masked sumcheck dimensions");
    return 1 + variables * degree;
}
Fr twoPower(std::size_t exponent) {
    Fr value = 1;
    for (std::size_t i = 0; i < exponent; ++i)
        value += value;
    return value;
}
std::vector<Fr> totalWeights(std::size_t L, unsigned d) {
    std::vector<Fr> weights(coefficientCount(L, d), L ? twoPower(L - 1) : Fr(0));
    weights[0] = twoPower(L);
    return weights;
}
std::vector<Fr> pointWeights(std::size_t L, unsigned d, const std::vector<Fr> &point) {
    if (point.size() != L)
        throw std::invalid_argument("mask terminal point size");
    std::vector<Fr> weights(coefficientCount(L, d), Fr(0));
    weights[0] = 1;
    for (std::size_t j = 0; j < L; ++j) {
        Fr power = 1;
        for (unsigned k = 1; k <= d; ++k) {
            power *= point[j];
            weights[1 + j * d + k - 1] = power;
        }
    }
    return weights;
}
std::vector<Fr> roundWeights(std::size_t L, unsigned d, const std::vector<Fr> &prefix,
                             const Fr &x) {
    const auto j = prefix.size();
    if (j >= L)
        throw std::invalid_argument("too many mask rounds");
    const Fr scale = twoPower(L - j - 1);
    std::vector<Fr> weights(coefficientCount(L, d), Fr(0));
    weights[0] = scale;
    for (std::size_t i = 0; i <= j; ++i) {
        Fr power = 1;
        const Fr &r = i == j ? x : prefix[i];
        for (unsigned k = 1; k <= d; ++k) {
            power *= r;
            weights[1 + i * d + k - 1] = scale * power;
        }
    }
    if (j + 1 < L) {
        const Fr tail = twoPower(L - j - 2);
        for (std::size_t i = 1 + (j + 1) * d; i < weights.size(); ++i)
            weights[i] = tail;
    }
    return weights;
}
ScalarWitness combineWitness(const std::vector<ScalarWitness> &values,
                             const std::vector<Fr> &weights) {
    if (values.size() != weights.size())
        throw std::invalid_argument("mask witness dimensions");
    ScalarWitness result;
    for (std::size_t i = 0; i < values.size(); ++i) {
        result.value += weights[i] * values[i].value;
        result.blinder += weights[i] * values[i].blinder;
    }
    return result;
}
G1 combineCommitment(const std::vector<G1> &values, const std::vector<Fr> &weights) {
    if (values.size() != weights.size())
        throw std::invalid_argument("mask commitment dimensions");
    G1 result;
    result.clear();
    for (std::size_t i = 0; i < values.size(); ++i)
        result += values[i] * weights[i];
    return result;
}
Fr interpolate(const std::vector<Fr> &values, const Fr &x) {
    Fr value = 0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        Fr basis = 1;
        for (std::size_t j = 0; j < values.size(); ++j)
            if (i != j)
                basis *= (x - Fr(j)) / (Fr(i) - Fr(j));
        value += values[i] * basis;
    }
    return value;
}
void bindMaskStatement(const Parameters &pp, std::size_t L, unsigned d, const G1 &initial,
                       Transcript &t, const std::string &label, const std::string &context) {
    coefficientCount(L, d);
    validateParameters(pp);
    requirePoint(initial);
    bindContext(t, pp, "masked-sumcheck", label, context);
    t.appendU64("sumcheck.variables", L);
    t.appendU64("sumcheck.degree", d);
    t.appendG1("sumcheck.initial", initial);
}
void appendMaskedRound(Transcript &t, const std::string &label, std::size_t j,
                       const std::vector<Fr> &values) {
    t.appendU64(label + ".round.index", j);
    for (const auto &value : values) {
        requireScalar(value);
        t.appendFr(label + ".round.value", value);
    }
}
void checkShape(const MaskedSumcheckProof &p, std::size_t L, unsigned d) {
    const auto n = coefficientCount(L, d);
    if (p.coefficients.size() != n || p.coefficient_knowledge.A.size() != n ||
        p.coefficient_knowledge.z_value.size() != n ||
        p.coefficient_knowledge.z_blind.size() != n || p.rounds.size() != L)
        throw std::invalid_argument("masked sumcheck proof dimensions");
    for (const auto &c : p.coefficients)
        requirePoint(c);
    requireScalar(p.masked_total);
    for (const auto &round : p.rounds)
        for (unsigned k = 0; k < 4; ++k) {
            requireScalar(round[k]);
            if (k > d && !round[k].isZero())
                throw std::invalid_argument("nonzero unused round slot");
        }
}
void requireVerified(bool accepted, const std::string &error) {
    if (!accepted)
        throw std::invalid_argument(error.empty() ? "masked sumcheck relation failed" : error);
}
std::size_t maskBytes(std::size_t L, unsigned d) {
    return 24 + 128 * coefficientCount(L, d) + 160 + 32 * (d + 1) * L;
}
void writeEquality(Writer &w, const EqualityProof &p) {
    w.point(p.A);
    w.fr(p.z);
}
EqualityProof readEquality(Reader &r) {
    EqualityProof p;
    p.A = r.point();
    p.z = r.fr();
    return p;
}
void writeRepresentation(Writer &w, const RepresentationProof &p, std::size_t n) {
    if (p.A.size() != n || p.z_value.size() != n || p.z_blind.size() != n)
        throw std::invalid_argument("representation codec shape");
    for (std::size_t i = 0; i < n; ++i) {
        w.point(p.A[i]);
        w.fr(p.z_value[i]);
        w.fr(p.z_blind[i]);
    }
}
RepresentationProof readRepresentation(Reader &r, std::size_t n) {
    RepresentationProof p;
    p.A.reserve(n);
    p.z_value.reserve(n);
    p.z_blind.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        p.A.push_back(r.point());
        p.z_value.push_back(r.fr());
        p.z_blind.push_back(r.fr());
    }
    return p;
}
void writeMask(Writer &w, const MaskedSumcheckProof &p, std::size_t L, unsigned d) {
    checkShape(p, L, d);
    w.u64(kMaskMagic);
    w.u64(L);
    w.u64(d);
    for (const auto &c : p.coefficients)
        w.point(c);
    writeRepresentation(w, p.coefficient_knowledge, p.coefficients.size());
    w.fr(p.masked_total);
    writeEquality(w, p.initial_relation);
    for (const auto &round : p.rounds)
        for (unsigned k = 0; k <= d; ++k)
            w.fr(round[k]);
    writeEquality(w, p.terminal_relation);
}
MaskedSumcheckProof readMask(Reader &r, std::size_t L, unsigned d) {
    const auto n = coefficientCount(L, d);
    if (r.u64() != kMaskMagic || r.u64() != L || r.u64() != d)
        throw std::invalid_argument("masked sumcheck codec version/dimensions");
    MaskedSumcheckProof p;
    p.coefficients.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        p.coefficients.push_back(r.point());
    p.coefficient_knowledge = readRepresentation(r, n);
    p.masked_total = r.fr();
    p.initial_relation = readEquality(r);
    p.rounds.resize(L);
    for (auto &round : p.rounds) {
        round.fill(Fr(0));
        for (unsigned k = 0; k <= d; ++k)
            round[k] = r.fr();
    }
    p.terminal_relation = readEquality(r);
    return p;
}
void writeProduct(Writer &w, const ProductProof &p) {
    w.point(p.A);
    w.point(p.B);
    w.fr(p.sx);
    w.fr(p.sr);
    w.fr(p.st);
}
ProductProof readProduct(Reader &r) {
    ProductProof p;
    p.A = r.point();
    p.B = r.point();
    p.sx = r.fr();
    p.sr = r.fr();
    p.st = r.fr();
    return p;
}
} // namespace

MaskedSumcheckProver::MaskedSumcheckProver(const Parameters &pp, std::size_t L, unsigned d,
                                           const G1 &initial, const ScalarWitness &witness,
                                           MaskedSumcheckProof &proof, Transcript &t,
                                           std::string label, std::string context)
    : pp_(pp), variables_(L), degree_(d), proof_(proof), transcript_(t), label_(std::move(label)),
      context_(std::move(context)), raw_claim_(witness.value) {
    const auto count = coefficientCount(L, d);
    bindMaskStatement(pp, L, d, initial, t, label_, context_);
    if (scalarCommit(pp, witness) != initial)
        throw std::invalid_argument("initial witness does not open claim");
    proof_ = MaskedSumcheckProof{};
    coefficients_.reserve(count);
    proof_.coefficients.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        coefficients_.push_back({sampleScalar(), sampleScalar()});
        proof_.coefficients.push_back(scalarCommit(pp, coefficients_.back()));
    }
    proof_.coefficient_knowledge = proveRepresentation(pp, proof_.coefficients, coefficients_, t,
                                                       label_ + ".mask-knowledge", context_);
    rho_ = t.challengeUniform(label_ + ".mask-rho");
    const auto weights = totalWeights(L, d);
    const auto G = combineWitness(coefficients_, weights);
    const auto DG = combineCommitment(proof_.coefficients, weights);
    masked_claim_ = witness.value + rho_ * G.value;
    proof_.masked_total = masked_claim_;
    t.appendFr(label_ + ".masked-total", masked_claim_);
    proof_.initial_relation =
        proveEquality(pp, {initial, DG}, {Fr(1), rho_}, masked_claim_, {witness, G}, t,
                      label_ + ".initial-relation", context_);
    point_.reserve(L);
    proof_.rounds.reserve(L);
}
Fr MaskedSumcheckProver::round(const std::vector<Fr> &raw) {
    if (finished_ || point_.size() >= variables_ || raw.size() != degree_ + 1)
        throw std::invalid_argument("invalid masked prover round state");
    for (const auto &value : raw)
        requireScalar(value);
    if (raw[0] + raw[1] != raw_claim_)
        throw std::logic_error("raw prover sumcheck consistency failed");
    std::vector<Fr> masked(degree_ + 1);
    std::array<Fr, 4> wire;
    wire.fill(Fr(0));
    for (unsigned k = 0; k <= degree_; ++k) {
        const auto mask =
            combineWitness(coefficients_, roundWeights(variables_, degree_, point_, Fr(k)));
        wire[k] = masked[k] = raw[k] + rho_ * mask.value;
    }
    if (masked[0] + masked[1] != masked_claim_)
        throw std::logic_error("mask formula consistency failed");
    appendMaskedRound(transcript_, label_, point_.size(), masked);
    proof_.rounds.push_back(wire);
    const Fr r = transcript_.challengeUniform(label_ + ".round.challenge", false);
    masked_claim_ = interpolate(masked, r);
    raw_claim_ = interpolate(raw, r);
    point_.push_back(r);
    return r;
}
void MaskedSumcheckProver::finish(const G1 &terminal, const ScalarWitness &witness) {
    if (finished_ || point_.size() != variables_ || witness.value != raw_claim_)
        throw std::logic_error("masked prover terminal/state mismatch");
    if (scalarCommit(pp_, witness) != terminal)
        throw std::invalid_argument("terminal witness commitment mismatch");
    const auto weights = pointWeights(variables_, degree_, point_);
    const auto g = combineWitness(coefficients_, weights);
    const auto Dg = combineCommitment(proof_.coefficients, weights);
    proof_.terminal_relation =
        proveEquality(pp_, {terminal, Dg}, {Fr(1), rho_}, masked_claim_, {witness, g}, transcript_,
                      label_ + ".terminal-relation", context_);
    finished_ = true;
}
MaskedSumcheckVerifier::MaskedSumcheckVerifier(const Parameters &pp, std::size_t L, unsigned d,
                                               const G1 &initial, const MaskedSumcheckProof &proof,
                                               Transcript &t, std::string label,
                                               std::string context)
    : pp_(pp), variables_(L), degree_(d), proof_(proof), transcript_(t), label_(std::move(label)),
      context_(std::move(context)) {
    checkShape(proof, L, d);
    bindMaskStatement(pp, L, d, initial, t, label_, context_);
    std::string error;
    requireVerified(verifyRepresentation(pp, proof.coefficients, proof.coefficient_knowledge, t,
                                         label_ + ".mask-knowledge", context_, &error),
                    error);
    rho_ = t.challengeUniform(label_ + ".mask-rho");
    masked_claim_ = proof.masked_total;
    t.appendFr(label_ + ".masked-total", masked_claim_);
    const auto DG = combineCommitment(proof.coefficients, totalWeights(L, d));
    error.clear();
    requireVerified(verifyEquality(pp, {initial, DG}, {Fr(1), rho_}, masked_claim_,
                                   proof.initial_relation, t, label_ + ".initial-relation",
                                   context_, &error),
                    error);
    point_.reserve(L);
}
Fr MaskedSumcheckVerifier::round() {
    if (finished_ || point_.size() >= variables_)
        throw std::invalid_argument("invalid masked verifier round state");
    const auto &wire = proof_.rounds.at(point_.size());
    std::vector<Fr> values(wire.begin(), wire.begin() + degree_ + 1);
    if (values[0] + values[1] != masked_claim_)
        throw std::invalid_argument("masked round consistency failed");
    appendMaskedRound(transcript_, label_, point_.size(), values);
    const Fr r = transcript_.challengeUniform(label_ + ".round.challenge", false);
    masked_claim_ = interpolate(values, r);
    point_.push_back(r);
    return r;
}
void MaskedSumcheckVerifier::finish(const G1 &terminal) {
    if (finished_ || point_.size() != variables_)
        throw std::invalid_argument("masked verifier terminal/state mismatch");
    const auto Dg =
        combineCommitment(proof_.coefficients, pointWeights(variables_, degree_, point_));
    std::string error;
    requireVerified(verifyEquality(pp_, {terminal, Dg}, {Fr(1), rho_}, masked_claim_,
                                   proof_.terminal_relation, transcript_,
                                   label_ + ".terminal-relation", context_, &error),
                    error);
    finished_ = true;
}
Fr equalityPolynomial(const std::vector<Fr> &a, const std::vector<Fr> &b) {
    if (a.size() > 30 || b.size() > 30)
        throw std::invalid_argument("equality point limit");
    for (const auto &x : a)
        requireScalar(x);
    for (const auto &x : b)
        requireScalar(x);
    return equalityEvaluation(a, b);
}
Degree1ZkResult proveDegree1SumcheckZk(const Parameters &pp, const std::vector<Fr> &input,
                                       const G1 &initial, const ScalarWitness &H, Transcript &t,
                                       const std::string &label, const std::string &context) {
    const auto L = exactLog2(input.size());
    if (L>30)throw std::invalid_argument("degree1 vector dimension limit");
    coefficientCount(L, 1);
    Fr sum = 0;
    for (const auto &value : input) {
        requireScalar(value);
        sum += value;
    }
    if (sum != H.value)
        throw std::logic_error("degree1 ZK witness does not match initial claim");
    Degree1ZkResult result;
    MaskedSumcheckProver mask(pp, L, 1, initial, H, result.proof.masked, t, label, context);
    std::vector<Fr> values = input;
    while (values.size() > 1) {
        const auto round = degree1Round(values);
        const auto r = mask.round({round.value_at_0, round.value_at_1});
        foldValues(values, r);
    }
    result.terminal_witness = {values[0], sampleScalar()};
    result.proof.terminal = scalarCommit(pp, result.terminal_witness);
    result.proof.terminal_knowledge =
        proveRepresentation(pp, {result.proof.terminal}, {result.terminal_witness}, t,
                            label + ".terminal-knowledge", context);
    mask.finish(result.proof.terminal, result.terminal_witness);
    result.point = mask.point();
    return result;
}
bool verifyDegree1SumcheckZk(const Parameters &pp, std::size_t L, const G1 &initial,
                             const Degree1ZkProof &proof, Transcript &t, const std::string &label,
                             const std::string &context, std::vector<Fr> *point,
                             std::string *error) {
    try {
        if (L>30)throw std::invalid_argument("degree1 vector dimension limit");
        MaskedSumcheckVerifier mask(pp, L, 1, initial, proof.masked, t, label, context);
        for (std::size_t j = 0; j < L; ++j)
            mask.round();
        std::string relation_error;
        requireVerified(verifyRepresentation(pp, {proof.terminal}, proof.terminal_knowledge, t,
                                             label + ".terminal-knowledge", context,
                                             &relation_error),
                        relation_error);
        mask.finish(proof.terminal);
        if (point)
            *point = mask.point();
        return true;
    } catch (const std::exception &e) {
        return failWith(error, e.what());
    }
}
Degree3ZkResult proveDegree3SumcheckZk(const Parameters &pp, const std::vector<Fr> &eq_point,
                                       const std::vector<Fr> &f_input,
                                       const std::vector<Fr> &g_input, const G1 &initial,
                                       const ScalarWitness &H, Transcript &t,
                                       const std::string &label, const std::string &context) {
    const auto L = exactLog2(f_input.size());
    if (L>30)throw std::invalid_argument("degree3 vector dimension limit");
    coefficientCount(L, 3);
    if (g_input.size() != f_input.size() || eq_point.size() != L)
        throw std::invalid_argument("degree3 ZK dimensions");
    for (const auto &value : eq_point)
        requireScalar(value);
    std::vector<Fr> equality = eqWeights(eq_point);
    Fr sum = 0;
    for (std::size_t i = 0; i < f_input.size(); ++i) {
        requireScalar(f_input[i]);
        requireScalar(g_input[i]);
        sum += equality[i] * f_input[i] * g_input[i];
    }
    if (sum != H.value)
        throw std::logic_error("degree3 ZK witness does not match initial claim");
    Degree3ZkResult result;
    // Equality point is part of the polynomial's public definition, before masks/rho.
    t.appendU64(label + ".equality-point-size", L);
    for (const auto &value : eq_point)
        t.appendFr(label + ".equality-point", value);
    MaskedSumcheckProver mask(pp, L, 3, initial, H, result.proof.masked, t, label, context);
    std::vector<Fr> f = f_input, g = g_input;
    while (f.size() > 1) {
        const auto round = degree3Round(equality, f, g);
        const auto r =
            mask.round({round.value_at_0, round.value_at_1, round.value_at_2, round.value_at_3});
        foldValues(equality, r);
        foldValues(f, r);
        foldValues(g, r);
    }
    result.f_witness = {f[0], sampleScalar()};
    result.g_witness = {g[0], sampleScalar()};
    result.proof.f_terminal = scalarCommit(pp, result.f_witness);
    result.proof.g_terminal = scalarCommit(pp, result.g_witness);
    result.proof.terminal_knowledge = proveRepresentation(
        pp, {result.proof.f_terminal, result.proof.g_terminal},
        {result.f_witness, result.g_witness}, t, label + ".terminal-knowledge", context);
    ScalarWitness product{f[0] * g[0], sampleScalar()};
    result.proof.product_commitment = scalarCommit(pp, product);
    result.proof.product = proveProduct(
        pp, result.proof.f_terminal, result.proof.g_terminal, result.proof.product_commitment,
        result.f_witness, result.g_witness, product, t, label + ".terminal-product", context);
    const Fr factor = equalityPolynomial(eq_point, mask.point());
    mask.finish(result.proof.product_commitment * factor,
                {product.value * factor, product.blinder * factor});
    result.point = mask.point();
    return result;
}
bool verifyDegree3SumcheckZk(const Parameters &pp, const std::vector<Fr> &eq_point,
                             const G1 &initial, const Degree3ZkProof &proof, Transcript &t,
                             const std::string &label, const std::string &context,
                             std::vector<Fr> *point, std::string *error) {
    try {
        const auto L = eq_point.size();
        coefficientCount(L, 3);
        t.appendU64(label + ".equality-point-size", L);
        for (const auto &value : eq_point) {
            requireScalar(value);
            t.appendFr(label + ".equality-point", value);
        }
        MaskedSumcheckVerifier mask(pp, L, 3, initial, proof.masked, t, label, context);
        for (std::size_t j = 0; j < L; ++j)
            mask.round();
        std::string relation_error;
        requireVerified(
            verifyRepresentation(pp, {proof.f_terminal, proof.g_terminal}, proof.terminal_knowledge,
                                 t, label + ".terminal-knowledge", context, &relation_error),
            relation_error);
        relation_error.clear();
        requireVerified(verifyProduct(pp, proof.f_terminal, proof.g_terminal,
                                      proof.product_commitment, proof.product, t,
                                      label + ".terminal-product", context, &relation_error),
                        relation_error);
        mask.finish(proof.product_commitment * equalityPolynomial(eq_point, mask.point()));
        if (point)
            *point = mask.point();
        return true;
    } catch (const std::exception &e) {
        return failWith(error, e.what());
    }
}
Bytes encodeMaskedSumcheck(const MaskedSumcheckProof &p, std::size_t L, unsigned d) {
    Writer w(maskBytes(L, d));
    writeMask(w, p, L, d);
    return w.finish();
}
std::size_t maskedSumcheckEncodedSize(std::size_t L,unsigned d) {return maskBytes(L,d);}
MaskedSumcheckProof decodeMaskedSumcheck(const Bytes &bytes, std::size_t L, unsigned d) {
    Reader r(bytes, maskBytes(L, d));
    auto p = readMask(r, L, d);
    r.finish();
    return p;
}
Bytes encodeDegree1Zk(const Degree1ZkProof &p, std::size_t L) {
    Writer w(8 + maskBytes(L, 1) + 128);
    w.u64(kDegree1Magic);
    writeMask(w, p.masked, L, 1);
    w.point(p.terminal);
    writeRepresentation(w, p.terminal_knowledge, 1);
    return w.finish();
}
Degree1ZkProof decodeDegree1Zk(const Bytes &bytes, std::size_t L) {
    Reader r(bytes, 8 + maskBytes(L, 1) + 128);
    if (r.u64() != kDegree1Magic)
        throw std::invalid_argument("degree1 ZK codec version");
    Degree1ZkProof p;
    p.masked = readMask(r, L, 1);
    p.terminal = r.point();
    p.terminal_knowledge = readRepresentation(r, 1);
    r.finish();
    return p;
}
Bytes encodeDegree3Zk(const Degree3ZkProof &p, std::size_t L) {
    Writer w(8 + maskBytes(L, 3) + 448);
    w.u64(kDegree3Magic);
    writeMask(w, p.masked, L, 3);
    w.point(p.f_terminal);
    w.point(p.g_terminal);
    w.point(p.product_commitment);
    writeRepresentation(w, p.terminal_knowledge, 2);
    writeProduct(w, p.product);
    return w.finish();
}
Degree3ZkProof decodeDegree3Zk(const Bytes &bytes, std::size_t L) {
    Reader r(bytes, 8 + maskBytes(L, 3) + 448);
    if (r.u64() != kDegree3Magic)
        throw std::invalid_argument("degree3 ZK codec version");
    Degree3ZkProof p;
    p.masked = readMask(r, L, 3);
    p.f_terminal = r.point();
    p.g_terminal = r.point();
    p.product_commitment = r.point();
    p.terminal_knowledge = readRepresentation(r, 2);
    p.product = readProduct(r);
    r.finish();
    return p;
}
} // namespace zkgpt::zk
