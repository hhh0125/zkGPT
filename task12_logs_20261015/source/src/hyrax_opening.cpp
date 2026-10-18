#include "hyrax_opening.hpp"
#include "thread_joiner.hpp"
#include "memory_diagnostics.hpp"
#include <limits>

#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <thread>

std::vector<Fr> hyraxMleEqualityWeights(const std::vector<Fr> &point,
                          std::size_t begin, std::size_t count) {
    if (count>30 || begin>point.size() || count>point.size()-begin)
        throw std::invalid_argument("invalid Hyrax equality-weight dimensions");
    for (std::size_t i=begin;i<begin+count;++i)
        if (!point[i].isValid()) throw std::invalid_argument("invalid Hyrax point scalar");
    std::vector<Fr> weights(static_cast<std::size_t>(1)<<count);
    weights[0]=1;
    std::size_t active=1;
    for (std::size_t bit=0;bit<count;++bit) {
        const Fr challenge=point.at(begin+bit);
        for (std::size_t i=active;i>0;--i) {
            const Fr previous=weights[i-1];
            weights[i-1]=previous*(Fr(1)-challenge);
            weights[i-1+active]=previous*challenge;
        }
        active*=2;
    }
    return weights;
}

namespace {

bool failWith(std::string *error, const std::string &message) {
    if (error) *error=message;
    return false;
}

bool isPowerOfTwo(std::size_t value) {
    return value!=0 && (value&(value-1))==0;
}


Fr innerProduct(const std::vector<Fr> &lhs,
                const std::vector<Fr> &rhs) {
    if (lhs.size()!=rhs.size())
        throw std::invalid_argument("inner-product vector length mismatch");
    Fr result=0;
    for (std::size_t i=0;i<lhs.size();++i) result+=lhs[i]*rhs[i];
    return result;
}

G1 commitVector(const std::vector<G1> &generators,
                const std::vector<Fr> &values) {
    if (generators.size()!=values.size())
        throw std::invalid_argument("commitment vector length mismatch");
    G1 result;
    G1::mulVec(result, const_cast<G1 *>(generators.data()), values.data(),
               values.size());
    return result;
}

void appendOpeningStatement(Transcript &transcript, const std::string &label,
                            std::size_t size, const G1 &commitment,
                            const Fr &evaluation) {
    transcript.appendString("ipa.label", label);
    transcript.appendU64("ipa.size", size);
    transcript.appendG1("ipa.commitment", commitment);
    transcript.appendFr("ipa.evaluation", evaluation);
}

// Runs body(begin, end) over [0, count) on joined worker threads. Used
// for the O(rows * columns) collapse loops, which dominate opening time
// for 2^22-sized witnesses.
template <typename Body>
void parallelRange(std::size_t count, Body body) {
    const std::size_t threads=std::min<std::size_t>(
        count<64 ? 1 : zkgpt::workerThreadCount(), count ? count : 1);
    if (threads<=1) {
        body(std::size_t{0}, count);
        return;
    }
    std::vector<std::thread> workers;
    workers.reserve(threads);
    zkgpt::ThreadJoiner joiner(workers);
    for (std::size_t t=0;t<threads;++t)
        workers.emplace_back(body, count*t/threads, count*(t+1)/threads);
    joiner.join();
}

struct MleReduction {
    G1 commitment;
    std::vector<Fr> coefficients;
    std::vector<Fr> witness;
};

MleReduction reduceMleProver(
    const std::vector<std::uint16_t> &values,
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators) {
    const std::size_t log_size=point.size();
    const std::size_t row_bits=log_size/2;
    const std::size_t column_bits=log_size-row_bits;
    const std::size_t row_count=static_cast<std::size_t>(1)<<row_bits;
    const std::size_t column_count=static_cast<std::size_t>(1)<<column_bits;
    if (values.size()!=row_count*column_count ||
        row_commitments.size()!=row_count || generators.size()<column_count)
        throw std::invalid_argument("invalid Hyrax MLE opening shape");

    const auto row_weights=hyraxMleEqualityWeights(point, 0, row_bits);
    auto column_weights=hyraxMleEqualityWeights(point, row_bits, column_bits);
    std::vector<Fr> collapsed(column_count, Fr(0));
    parallelRange(column_count, [&](std::size_t begin, std::size_t end) {
        for (std::size_t column=begin;column<end;++column)
            for (std::size_t row=0;row<row_count;++row)
                collapsed[column]+=row_weights[row]*
                    Fr(values[row+column*row_count]);
    });

    G1 commitment;
    G1::mulVec(commitment, const_cast<G1 *>(row_commitments.data()),
               row_weights.data(),
               row_count);
    return {commitment, std::move(column_weights), std::move(collapsed)};
}

MleReduction reduceMleProverFr(
    const std::vector<Fr> &values,
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators) {
    const std::size_t log_size=point.size();
    const std::size_t row_bits=log_size/2;
    const std::size_t column_bits=log_size-row_bits;
    const std::size_t row_count=static_cast<std::size_t>(1)<<row_bits;
    const std::size_t column_count=static_cast<std::size_t>(1)<<column_bits;
    if (values.size()!=row_count*column_count ||
        row_commitments.size()!=row_count || generators.size()<column_count)
        throw std::invalid_argument("invalid field Hyrax MLE opening shape");

    const auto row_weights=hyraxMleEqualityWeights(point, 0, row_bits);
    auto column_weights=hyraxMleEqualityWeights(point, row_bits, column_bits);
    std::vector<Fr> collapsed(column_count, Fr(0));
    parallelRange(column_count, [&](std::size_t begin, std::size_t end) {
        for (std::size_t column=begin;column<end;++column)
            for (std::size_t row=0;row<row_count;++row)
                collapsed[column]+=row_weights[row]*
                    values[row+column*row_count];
    });
    G1 commitment;
    G1::mulVec(commitment, const_cast<G1 *>(row_commitments.data()),
               row_weights.data(), row_count);
    return {commitment, std::move(column_weights), std::move(collapsed)};
}

MleReduction reduceMleProverFrRowMajor(
    HyraxFrWitnessView values,
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators) {
    const std::size_t log_size=point.size();
    if (log_size>30 || log_size>=std::numeric_limits<std::size_t>::digits)
        throw std::invalid_argument("unsupported row-major Hyrax log size");
    const std::size_t value_count=std::size_t{1}<<log_size;
    if (value_count>std::numeric_limits<std::size_t>::max()/sizeof(Fr))
        throw std::invalid_argument("row-major Hyrax witness byte size overflow");
    const std::size_t row_bits=log_size/2;
    const std::size_t column_bits=log_size-row_bits;
    const std::size_t row_count=static_cast<std::size_t>(1)<<row_bits;
    const std::size_t column_count=static_cast<std::size_t>(1)<<column_bits;
    if (row_count>std::numeric_limits<std::size_t>::max()/column_count ||
        row_count*column_count!=value_count || values.size()!=value_count ||
        row_commitments.size()!=row_count || generators.size()<column_count)
        throw std::invalid_argument("invalid row-major Hyrax MLE opening shape");

    const auto column_weights=hyraxMleEqualityWeights(point, 0, column_bits);
    const auto row_weights=hyraxMleEqualityWeights(point, column_bits, row_bits);
    std::vector<Fr> collapsed(column_count, Fr(0));
    zkgpt::memory_diagnostics::logVector("input_opening_reduction_allocated","collapsed",collapsed);
    zkgpt::memory_diagnostics::logProcess("input_opening_reduction_allocated");
    parallelRange(column_count, [&](std::size_t begin, std::size_t end) {
        for (std::size_t row=0;row<row_count;++row)
            for (std::size_t column=begin;column<end;++column)
                collapsed[column]+=row_weights[row]*
                    values[row*column_count+column];
    });
    G1 commitment;
    G1::mulVec(commitment, const_cast<G1 *>(row_commitments.data()),
               row_weights.data(), row_count);
    return {commitment, column_weights, std::move(collapsed)};
}

std::pair<G1, std::vector<Fr>> reduceMleVerifier(
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators) {
    const std::size_t log_size=point.size();
    const std::size_t row_bits=log_size/2;
    const std::size_t column_bits=log_size-row_bits;
    const std::size_t row_count=static_cast<std::size_t>(1)<<row_bits;
    const std::size_t column_count=static_cast<std::size_t>(1)<<column_bits;
    if (row_commitments.size()!=row_count || generators.size()<column_count)
        throw std::invalid_argument("invalid public Hyrax MLE opening shape");
    const auto row_weights=hyraxMleEqualityWeights(point, 0, row_bits);
    auto column_weights=hyraxMleEqualityWeights(point, row_bits, column_bits);
    G1 commitment;
    G1::mulVec(commitment, const_cast<G1 *>(row_commitments.data()),
               row_weights.data(),
               row_count);
    return {commitment, std::move(column_weights)};
}

std::pair<G1, std::vector<Fr>> reduceMleVerifierRowMajor(
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators) {
    const std::size_t log_size=point.size();
    const std::size_t row_bits=log_size/2;
    const std::size_t column_bits=log_size-row_bits;
    const std::size_t row_count=static_cast<std::size_t>(1)<<row_bits;
    const std::size_t column_count=static_cast<std::size_t>(1)<<column_bits;
    if (row_commitments.size()!=row_count || generators.size()<column_count)
        throw std::invalid_argument(
            "invalid public row-major Hyrax MLE opening shape");
    const auto column_weights=hyraxMleEqualityWeights(point, 0, column_bits);
    const auto row_weights=hyraxMleEqualityWeights(point, column_bits, row_bits);
    G1 commitment;
    G1::mulVec(commitment, const_cast<G1 *>(row_commitments.data()),
               row_weights.data(), row_count);
    return {commitment, column_weights};
}

}  // namespace

HyraxIpaProof hyraxInnerProductProve(
    const std::vector<Fr> &witness_input,
    const std::vector<Fr> &coefficient_input,
    const std::vector<G1> &generator_input, const G1 &u,
    const G1 &commitment, const Fr &evaluation, Transcript &transcript,
    const std::string &label, bool check_witness_commitment) {
    if (!isPowerOfTwo(witness_input.size()) ||
        witness_input.size()!=coefficient_input.size() ||
        witness_input.size()!=generator_input.size())
        throw std::invalid_argument("invalid IPA statement shape");
    if (innerProduct(witness_input, coefficient_input)!=evaluation)
        throw std::logic_error("IPA witness does not match claimed evaluation");
    if (check_witness_commitment &&
        commitVector(generator_input, witness_input)!=commitment)
        throw std::logic_error("IPA witness does not match commitment");

    appendOpeningStatement(transcript, label, witness_input.size(), commitment,
                           evaluation);
    std::vector<Fr> witness=witness_input;
    std::vector<Fr> coefficients=coefficient_input;
    std::vector<G1> generators=generator_input;
    HyraxIpaProof proof;
    while (witness.size()>1) {
        const std::size_t half=witness.size()/2;
        std::vector<Fr> witness_left(witness.begin(), witness.begin()+half);
        std::vector<Fr> witness_right(witness.begin()+half, witness.end());
        std::vector<Fr> coefficient_left(
            coefficients.begin(), coefficients.begin()+half);
        std::vector<Fr> coefficient_right(
            coefficients.begin()+half, coefficients.end());
        std::vector<G1> generator_left(
            generators.begin(), generators.begin()+half);
        std::vector<G1> generator_right(
            generators.begin()+half, generators.end());

        HyraxIpaRound round;
        round.left=commitVector(generator_right, witness_left)+
                   u*innerProduct(witness_left, coefficient_right);
        round.right=commitVector(generator_left, witness_right)+
                    u*innerProduct(witness_right, coefficient_left);
        proof.rounds.push_back(round);
        transcript.appendG1("ipa.left", round.left);
        transcript.appendG1("ipa.right", round.right);
        const Fr challenge=transcript.challenge("ipa-round");
        Fr inverse;
        Fr::inv(inverse, challenge);

        for (std::size_t i=0;i<half;++i) {
            witness[i]=challenge*witness_left[i]+inverse*witness_right[i];
            coefficients[i]=inverse*coefficient_left[i]+
                            challenge*coefficient_right[i];
            generators[i]=generator_left[i]*inverse+
                          generator_right[i]*challenge;
        }
        witness.resize(half);
        coefficients.resize(half);
        generators.resize(half);
    }
    proof.final_witness=witness[0];
    transcript.appendFr("ipa.final_witness", proof.final_witness);
    return proof;
}

bool hyraxInnerProductVerify(
    const std::vector<Fr> &coefficient_input,
    const std::vector<G1> &generator_input, const G1 &u,
    const G1 &commitment, const Fr &evaluation,
    const HyraxIpaProof &proof, Transcript &transcript,
    const std::string &label, std::string *error) {
    try {
        if (!isPowerOfTwo(coefficient_input.size()) ||
            coefficient_input.size()!=generator_input.size())
            return failWith(error, "invalid IPA verifier statement shape");
        std::size_t expected_rounds=0;
        for (std::size_t n=coefficient_input.size();n>1;n>>=1)
            ++expected_rounds;
        if (proof.rounds.size()!=expected_rounds)
            return failWith(error, "IPA round count mismatch");

        appendOpeningStatement(transcript, label, coefficient_input.size(),
                               commitment, evaluation);
        // Instead of folding the generators round by round (about 2n scalar
        // multiplications), collect the challenges and check the final
        // equation with multi-scalar multiplications:
        //   C + u*v + sum(c_j^2 L_j + c_j^-2 R_j) == <s, g>*w + u*(w*<s, a>)
        // where s_i = prod_j (bit of i split in round j ? c_j : c_j^-1).
        const std::size_t rounds=proof.rounds.size();
        std::vector<Fr> challenges(rounds), inverses(rounds);
        std::vector<G1> cross_points;
        std::vector<Fr> cross_scalars;
        cross_points.reserve(2*rounds);
        cross_scalars.reserve(2*rounds);
        for (std::size_t j=0;j<rounds;++j) {
            const auto &round=proof.rounds[j];
            transcript.appendG1("ipa.left", round.left);
            transcript.appendG1("ipa.right", round.right);
            challenges[j]=transcript.challenge("ipa-round");
            Fr::inv(inverses[j], challenges[j]);
            cross_points.push_back(round.left);
            cross_scalars.push_back(challenges[j]*challenges[j]);
            cross_points.push_back(round.right);
            cross_scalars.push_back(inverses[j]*inverses[j]);
        }
        transcript.appendFr("ipa.final_witness", proof.final_witness);

        // Round j splits on bit (rounds-1-j) of the original index.
        std::vector<Fr> scalars(coefficient_input.size());
        scalars[0]=1;
        for (std::size_t bit=0, active=1;bit<rounds;++bit, active*=2) {
            const std::size_t j=rounds-1-bit;
            for (std::size_t i=0;i<active;++i) {
                scalars[i+active]=scalars[i]*challenges[j];
                scalars[i]*=inverses[j];
            }
        }
        Fr folded_coefficient=0;
        for (std::size_t i=0;i<scalars.size();++i)
            folded_coefficient+=scalars[i]*coefficient_input[i];
        for (auto &scalar : scalars) scalar*=proof.final_witness;

        G1 folded_commitment=commitment+u*evaluation;
        if (!cross_points.empty()) {
            G1 cross;
            G1::mulVec(cross, cross_points.data(), cross_scalars.data(),
                       cross_points.size());
            folded_commitment+=cross;
        }
        G1 expected;
        G1::mulVec(expected, const_cast<G1 *>(generator_input.data()),
                   scalars.data(), scalars.size());
        expected+=u*(proof.final_witness*folded_coefficient);
        if (folded_commitment!=expected)
            return failWith(error, "IPA final commitment equation failed");
        return true;
    } catch (const std::exception &exception) {
        return failWith(error, exception.what());
    }
}

MleOpeningProof hyraxMleOpenProve(
    const std::vector<std::uint16_t> &values,
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators, const G1 &u,
    const Fr &evaluation, Transcript &transcript,
    const std::string &label) {
    auto reduction=reduceMleProver(values, row_commitments, point, generators);
    const Fr reduced_evaluation=innerProduct(
        reduction.witness, reduction.coefficients);
    if (reduced_evaluation!=evaluation) {
        std::ostringstream message;
        message << "MLE opening evaluation mismatch: reduced="
                << reduced_evaluation << ", claimed=" << evaluation;
        throw std::logic_error(message.str());
    }
    std::vector<G1> opening_generators(
        generators.begin(), generators.begin()+reduction.witness.size());
    MleOpeningProof proof;
    proof.ipa=hyraxInnerProductProve(
        reduction.witness, reduction.coefficients, opening_generators, u,
        reduction.commitment, evaluation, transcript, label);
    return proof;
}

MleOpeningProof hyraxMleOpenProveFr(
    const std::vector<Fr> &values,
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators, const G1 &u,
    const Fr &evaluation, Transcript &transcript,
    const std::string &label) {
    auto reduction=reduceMleProverFr(values, row_commitments, point, generators);
    const Fr reduced_evaluation=innerProduct(
        reduction.witness, reduction.coefficients);
    if (reduced_evaluation!=evaluation)
        throw std::logic_error("field MLE opening evaluation mismatch");
    std::vector<G1> opening_generators(
        generators.begin(), generators.begin()+reduction.witness.size());
    MleOpeningProof proof;
    proof.ipa=hyraxInnerProductProve(
        reduction.witness, reduction.coefficients, opening_generators, u,
        reduction.commitment, evaluation, transcript, label);
    return proof;
}

MleOpeningProof hyraxMleOpenProveFrRowMajor(
    HyraxFrWitnessView values,
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators, const G1 &u,
    const Fr &evaluation, Transcript &transcript,
    const std::string &label) {
    auto reduction=reduceMleProverFrRowMajor(
        values, row_commitments, point, generators);
    if (innerProduct(reduction.witness, reduction.coefficients)!=evaluation)
        throw std::logic_error("row-major field MLE evaluation mismatch");
    std::vector<G1> opening_generators(
        generators.begin(), generators.begin()+reduction.witness.size());
    MleOpeningProof proof;
    proof.ipa=hyraxInnerProductProve(
        reduction.witness, reduction.coefficients, opening_generators, u,
        reduction.commitment, evaluation, transcript, label);
    zkgpt::memory_diagnostics::logProcess("input_opening_after_ipa");
    return proof;
}

MleOpeningProof hyraxMleOpenProveFrRowMajor(
    const std::vector<Fr> &values,
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators, const G1 &u,
    const Fr &evaluation, Transcript &transcript,
    const std::string &label) {
    return hyraxMleOpenProveFrRowMajor(
        HyraxFrWitnessView(values.data(),values.size()),row_commitments,point,
        generators,u,evaluation,transcript,label);
}

bool hyraxMleOpenVerify(
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators, const G1 &u,
    const Fr &evaluation, const MleOpeningProof &proof,
    Transcript &transcript, const std::string &label,
    std::string *error) {
    try {
        auto reduction=reduceMleVerifier(row_commitments, point, generators);
        std::vector<G1> opening_generators(
            generators.begin(), generators.begin()+reduction.second.size());
        return hyraxInnerProductVerify(
            reduction.second, opening_generators, u, reduction.first,
            evaluation, proof.ipa, transcript, label, error);
    } catch (const std::exception &exception) {
        return failWith(error, exception.what());
    }
}

bool hyraxMleOpenVerifyRowMajor(
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators, const G1 &u,
    const Fr &evaluation, const MleOpeningProof &proof,
    Transcript &transcript, const std::string &label,
    std::string *error) {
    try {
        auto reduction=reduceMleVerifierRowMajor(
            row_commitments, point, generators);
        std::vector<G1> opening_generators(
            generators.begin(), generators.begin()+reduction.second.size());
        return hyraxInnerProductVerify(
            reduction.second, opening_generators, u, reduction.first,
            evaluation, proof.ipa, transcript, label, error);
    } catch (const std::exception &exception) {
        return failWith(error, exception.what());
    }
}
