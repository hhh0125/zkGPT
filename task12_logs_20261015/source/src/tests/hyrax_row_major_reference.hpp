#pragma once
// Frozen Task 7 C vector row-major path. Test-only: never linked into gpt_lib.
#include "hyrax_opening.hpp"
#include "thread_joiner.hpp"
#include <algorithm>
#include <thread>
namespace hyrax_reference {
struct MleReduction { G1 commitment; std::vector<Fr> coefficients,witness; };
template <typename Body> void parallelRange(std::size_t count, Body body) {
    const std::size_t threads=std::min<std::size_t>(count<64?1:zkgpt::workerThreadCount(),count?count:1);
    if (threads<=1) { body(0,count); return; }
    std::vector<std::thread> workers;
    zkgpt::ThreadJoiner joiner(workers);
    for (std::size_t t=0;t<threads;++t) workers.emplace_back(body,count*t/threads,count*(t+1)/threads);
    joiner.join();
}
std::vector<Fr> eqWeights(const std::vector<Fr> &point,
                          std::size_t begin, std::size_t count) {
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

Fr innerProduct(const std::vector<Fr> &lhs,
                const std::vector<Fr> &rhs) {
    if (lhs.size()!=rhs.size())
        throw std::invalid_argument("inner-product vector length mismatch");
    Fr result=0;
    for (std::size_t i=0;i<lhs.size();++i) result+=lhs[i]*rhs[i];
    return result;
}

MleReduction referenceReduce(
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
        throw std::invalid_argument("invalid row-major Hyrax MLE opening shape");

    const auto column_weights=eqWeights(point, 0, column_bits);
    const auto row_weights=eqWeights(point, column_bits, row_bits);
    std::vector<Fr> collapsed(column_count, Fr(0));
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

MleOpeningProof referenceOpening(
    const std::vector<Fr> &values,
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators, const G1 &u,
    const Fr &evaluation, Transcript &transcript,
    const std::string &label) {
    auto reduction=referenceReduce(
        values, row_commitments, point, generators);
    if (innerProduct(reduction.witness, reduction.coefficients)!=evaluation)
        throw std::logic_error("row-major field MLE evaluation mismatch");
    std::vector<G1> opening_generators(
        generators.begin(), generators.begin()+reduction.witness.size());
    MleOpeningProof proof;
    proof.ipa=hyraxInnerProductProve(
        reduction.witness, reduction.coefficients, opening_generators, u,
        reduction.commitment, evaluation, transcript, label);
    return proof;
}

} // namespace hyrax_reference
