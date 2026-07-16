#include "sparse_opening.hpp"

#include "hyrax_opening.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <tuple>
#include <iostream>
#include <atomic>
#include <mutex>
#include <thread>

namespace {

bool failWith(std::string *error, const std::string &message) {
    if (error) *error=message;
    return false;
}

std::size_t exactLog2(std::size_t value) {
    if (value==0 || (value&(value-1))!=0)
        throw std::invalid_argument("sparse opening size is not a power of two");
    std::size_t result=0;
    while (value>1) {
        value>>=1;
        ++result;
    }
    return result;
}

std::vector<Fr> eqWeights(const std::vector<Fr> &point,
                          std::size_t begin, std::size_t count) {
    std::vector<Fr> weights(static_cast<std::size_t>(1)<<count, Fr(0));
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

Fr powerOfTwo(unsigned bits) {
    Fr value=1;
    for (unsigned i=0;i<bits;++i) value+=value;
    return value;
}

struct PatternKey {
    std::size_t val0_column_start;
    std::size_t query_low_start;
    std::size_t length;

    bool operator<(const PatternKey &other) const {
        return std::tie(val0_column_start, query_low_start, length)<
               std::tie(other.val0_column_start, other.query_low_start,
                        other.length);
    }
};

struct Occurrence {
    std::size_t val0_row;
    Fr scalar;
};

struct PatternLayout {
    PatternKey key;
    std::vector<Occurrence> occurrences;
};

struct SparseLayout {
    std::size_t val0_row_count = 0;
    std::size_t val0_column_count = 0;
    std::vector<Fr> query_low_weights;
    std::vector<PatternLayout> patterns;
    Fr signed_bias_evaluation = Fr(0);
};

SparseLayout buildLayout(const RangePublicStatement &statement,
                         std::size_t query_index,
                         const std::vector<Fr> &point) {
    if (query_index>=statement.queries.size())
        throw std::out_of_range("sparse opening query index is invalid");
    const auto &query=statement.queries[query_index];
    if (point.size()!=exactLog2(query.padded_query_size))
        throw std::invalid_argument("sparse opening point length mismatch");

    SparseLayout layout;
    const std::size_t val0_row_bits=statement.val0_log_size/2;
    const std::size_t val0_column_bits=
        statement.val0_log_size-val0_row_bits;
    layout.val0_row_count=static_cast<std::size_t>(1)<<val0_row_bits;
    layout.val0_column_count=static_cast<std::size_t>(1)<<val0_column_bits;
    const std::size_t query_low_bits=std::min(
        val0_column_bits, point.size());
    const std::size_t query_low_size=static_cast<std::size_t>(1)
                                     << query_low_bits;
    layout.query_low_weights=eqWeights(point, 0, query_low_bits);
    const auto query_high_weights=eqWeights(
        point, query_low_bits, point.size()-query_low_bits);

    std::map<PatternKey, std::vector<Occurrence>> grouped;
    for (const auto &region : statement.regions) {
        if (region.proof_constraint_index!=query_index) continue;
        const Fr bias=region.is_signed ? powerOfTwo(region.bits-1) : Fr(0);
        std::size_t consumed=0;
        while (consumed<region.count) {
            const std::size_t val0_index=region.val0_offset+consumed;
            const std::size_t query_position=region.proof_start+consumed;
            const std::size_t val0_row=val0_index/layout.val0_column_count;
            const std::size_t val0_column=val0_index%layout.val0_column_count;
            const std::size_t query_low=query_position%query_low_size;
            const std::size_t query_high=query_position/query_low_size;
            if (val0_row>=layout.val0_row_count ||
                query_high>=query_high_weights.size())
                throw std::out_of_range("sparse opening region exceeds capacity");
            const std::size_t length=std::min({
                region.count-consumed,
                layout.val0_column_count-val0_column,
                query_low_size-query_low});
            const Fr scalar=query_high_weights[query_high];
            const PatternKey key{val0_column, query_low, length};
            grouped[key].push_back({val0_row, scalar});
            if (region.is_signed) {
                Fr low_sum=0;
                for (std::size_t i=0;i<length;++i)
                    low_sum+=layout.query_low_weights[query_low+i];
                layout.signed_bias_evaluation+=scalar*low_sum*bias;
            }
            consumed+=length;
        }
    }
    layout.patterns.reserve(grouped.size());
    for (auto &entry : grouped)
        layout.patterns.push_back({entry.first, std::move(entry.second)});
    return layout;
}

std::vector<Fr> patternCoefficients(const SparseLayout &layout,
                                    const PatternKey &key) {
    std::vector<Fr> coefficients(layout.val0_column_count, Fr(0));
    for (std::size_t i=0;i<key.length;++i)
        coefficients[key.val0_column_start+i]=
            layout.query_low_weights[key.query_low_start+i];
    return coefficients;
}

G1 aggregateCommitment(const RangePublicStatement &statement,
                       const PatternLayout &pattern) {
    G1 commitment;
    commitment.clear();
    for (const auto &occurrence : pattern.occurrences)
        commitment+=statement.val0_commitment.at(occurrence.val0_row)*
                    occurrence.scalar;
    return commitment;
}

void appendSparseHeader(Transcript &transcript, std::size_t query_index,
                        const Fr &bias, const Fr &claimed,
                        std::size_t pattern_count) {
    transcript.appendU64("sparse.query", query_index);
    transcript.appendFr("sparse.signed_bias", bias);
    transcript.appendFr("sparse.claimed_inner_product", claimed);
    transcript.appendU64("sparse.pattern_count", pattern_count);
}

void appendPatternMetadata(Transcript &transcript, const PatternKey &key) {
    transcript.appendU64("sparse.val0_column_start", key.val0_column_start);
    transcript.appendU64("sparse.query_low_start", key.query_low_start);
    transcript.appendU64("sparse.length", key.length);
}

Transcript makePatternTranscript(const Fr &parent_seed,
                                 std::size_t query_index,
                                 std::size_t pattern_index,
                                 const PatternKey &key) {
    Transcript transcript("zkGPT-sparse-pattern-v2");
    transcript.appendFr("sparse.parent_seed",parent_seed);
    transcript.appendU64("sparse.query",query_index);
    transcript.appendU64("sparse.pattern_index",pattern_index);
    appendPatternMetadata(transcript,key);
    return transcript;
}

void appendPatternProof(Transcript &transcript,const PatternKey &key,
                        const SparsePatternOpeningProof &proof) {
    appendPatternMetadata(transcript,key);
    transcript.appendFr("sparse.pattern_evaluation",
                        proof.claimed_inner_product);
    transcript.appendU64("sparse.ipa_round_count",
                         proof.opening.rounds.size());
    for (const auto &round : proof.opening.rounds) {
        transcript.appendG1("sparse.ipa_left",round.left);
        transcript.appendG1("sparse.ipa_right",round.right);
    }
    transcript.appendFr("sparse.ipa_final_witness",
                        proof.opening.final_witness);
}

}  // namespace

SparseLinearOpeningProof proveSparseVal0Opening(
    const RangePublicStatement &statement, std::size_t query_index,
    const std::vector<Fr> &point, const std::vector<F> &val0_witness,
    const Fr &encoded_evaluation, Transcript &transcript) {
    const auto layout=buildLayout(statement, query_index, point);
    const auto &generator_set=getGeneratorSet(statement.val0_generator_domain);
    if (generator_set.generators.size()!=layout.val0_column_count)
        throw std::invalid_argument("sparse opening generator count mismatch");

    SparseLinearOpeningProof proof;
    proof.query_index=query_index;
    proof.signed_bias_evaluation=layout.signed_bias_evaluation;
    proof.claimed_inner_product=encoded_evaluation-
                                proof.signed_bias_evaluation;
    appendSparseHeader(transcript, query_index, proof.signed_bias_evaluation,
                       proof.claimed_inner_product, layout.patterns.size());
    const Fr pattern_seed=transcript.challenge("sparse.pattern.seed");

    proof.patterns.resize(layout.patterns.size());
    std::cout << "  sparse val0 opening query " << query_index
              << ": patterns=" << layout.patterns.size() << std::endl;
    std::atomic<std::size_t> next_pattern{0};
    std::exception_ptr worker_error;
    std::mutex error_mutex;
    const std::size_t thread_count=std::min<std::size_t>(
        8,std::max<std::size_t>(1,layout.patterns.size()));
    std::vector<std::thread> workers;
    workers.reserve(thread_count);
    for (std::size_t thread_id=0;thread_id<thread_count;++thread_id) {
        workers.emplace_back([&] {
            try {
                for (;;) {
                    const std::size_t pattern_index=next_pattern.fetch_add(1);
                    if (pattern_index>=layout.patterns.size()) break;
                    const auto &pattern=layout.patterns[pattern_index];
                    const auto coefficients=patternCoefficients(
                        layout,pattern.key);
                    std::vector<Fr> aggregate(
                        layout.val0_column_count,Fr(0));
                    for (const auto &occurrence : pattern.occurrences) {
                        const std::size_t row_offset=occurrence.val0_row*
                            layout.val0_column_count;
                        for (std::size_t column=0;
                             column<layout.val0_column_count;++column) {
                            const std::size_t index=row_offset+column;
                            if (index<val0_witness.size())
                                aggregate[column]+=occurrence.scalar*
                                                   val0_witness[index];
                        }
                    }
                    Fr evaluation=0;
                    for (std::size_t i=0;i<aggregate.size();++i)
                        evaluation+=aggregate[i]*coefficients[i];
                    SparsePatternOpeningProof pattern_proof;
                    pattern_proof.val0_column_start=
                        pattern.key.val0_column_start;
                    pattern_proof.query_low_start=
                        pattern.key.query_low_start;
                    pattern_proof.length=pattern.key.length;
                    pattern_proof.claimed_inner_product=evaluation;
                    const G1 commitment=aggregateCommitment(statement,pattern);
                    const std::string label="val0-sparse/"+
                        std::to_string(query_index)+"/"+
                        std::to_string(pattern_index);
                    auto pattern_transcript=makePatternTranscript(
                        pattern_seed,query_index,pattern_index,pattern.key);
                    pattern_proof.opening=hyraxInnerProductProve(
                        aggregate,coefficients,generator_set.generators,
                        generator_set.u,commitment,evaluation,
                        pattern_transcript,label,false);
                    proof.patterns[pattern_index]=std::move(pattern_proof);
                }
            } catch (...) {
                std::lock_guard<std::mutex> lock(error_mutex);
                if (!worker_error) worker_error=std::current_exception();
            }
        });
    }
    for (auto &worker : workers) worker.join();
    if (worker_error) std::rethrow_exception(worker_error);
    Fr total=0;
    for (std::size_t pattern_index=0;pattern_index<layout.patterns.size();
         ++pattern_index) {
        total+=proof.patterns[pattern_index].claimed_inner_product;
        appendPatternProof(transcript,layout.patterns[pattern_index].key,
                           proof.patterns[pattern_index]);
    }
    std::cout << "    proved sparse patterns " << layout.patterns.size()
              << "/" << layout.patterns.size() << " using " << thread_count
              << " threads" << std::endl;
    if (total!=proof.claimed_inner_product)
        throw std::logic_error("sparse val[0] mapping evaluation mismatch");
    return proof;
}

bool verifySparseVal0Opening(
    const RangePublicStatement &statement, std::size_t query_index,
    const std::vector<Fr> &point, const Fr &encoded_evaluation,
    const SparseLinearOpeningProof &proof, Transcript &transcript,
    std::string *error) {
    try {
        const auto layout=buildLayout(statement, query_index, point);
        const auto &generator_set=getGeneratorSet(
            statement.val0_generator_domain);
        if (proof.query_index!=query_index ||
            proof.patterns.size()!=layout.patterns.size())
            return failWith(error, "sparse opening metadata mismatch");
        if (proof.signed_bias_evaluation!=layout.signed_bias_evaluation)
            return failWith(error, "sparse signed bias evaluation mismatch");
        if (proof.claimed_inner_product!=encoded_evaluation-
                proof.signed_bias_evaluation)
            return failWith(error, "sparse encoded evaluation mismatch");
        appendSparseHeader(transcript, query_index,
                           proof.signed_bias_evaluation,
                           proof.claimed_inner_product,
                           layout.patterns.size());
        const Fr pattern_seed=transcript.challenge("sparse.pattern.seed");
        for (std::size_t pattern_index=0;pattern_index<layout.patterns.size();
             ++pattern_index) {
            const auto &pattern=layout.patterns[pattern_index];
            const auto &pattern_proof=proof.patterns[pattern_index];
            if (pattern_proof.val0_column_start!=
                    pattern.key.val0_column_start ||
                pattern_proof.query_low_start!=pattern.key.query_low_start ||
                pattern_proof.length!=pattern.key.length)
                return failWith(error, "sparse pattern metadata mismatch");
        }
        std::atomic<std::size_t> next_pattern{0};
        std::vector<std::string> opening_errors(layout.patterns.size());
        const std::size_t thread_count=std::min<std::size_t>(
            8,std::max<std::size_t>(1,layout.patterns.size()));
        std::vector<std::thread> workers;
        workers.reserve(thread_count);
        for (std::size_t thread_id=0;thread_id<thread_count;++thread_id) {
            workers.emplace_back([&] {
                for (;;) {
                    const std::size_t pattern_index=next_pattern.fetch_add(1);
                    if (pattern_index>=layout.patterns.size()) break;
                    try {
                        const auto &pattern=layout.patterns[pattern_index];
                        const auto &pattern_proof=proof.patterns[pattern_index];
                        const auto coefficients=patternCoefficients(
                            layout,pattern.key);
                        const G1 commitment=aggregateCommitment(
                            statement,pattern);
                        const std::string label="val0-sparse/"+
                            std::to_string(query_index)+"/"+
                            std::to_string(pattern_index);
                        auto pattern_transcript=makePatternTranscript(
                            pattern_seed,query_index,pattern_index,pattern.key);
                        if (!hyraxInnerProductVerify(
                                coefficients,generator_set.generators,
                                generator_set.u,commitment,
                                pattern_proof.claimed_inner_product,
                                pattern_proof.opening,pattern_transcript,label,
                                &opening_errors[pattern_index]) &&
                            opening_errors[pattern_index].empty())
                            opening_errors[pattern_index]=
                                "unknown IPA verification failure";
                    } catch (const std::exception &exception) {
                        opening_errors[pattern_index]=exception.what();
                    }
                }
            });
        }
        for (auto &worker : workers) worker.join();
        Fr total=0;
        for (std::size_t pattern_index=0;pattern_index<layout.patterns.size();
             ++pattern_index) {
            if (!opening_errors[pattern_index].empty())
                return failWith(error,"sparse pattern opening failed at "+
                    std::to_string(pattern_index)+": "+
                    opening_errors[pattern_index]);
            total+=proof.patterns[pattern_index].claimed_inner_product;
            appendPatternProof(transcript,layout.patterns[pattern_index].key,
                               proof.patterns[pattern_index]);
        }
        if (total!=proof.claimed_inner_product)
            return failWith(error, "sparse pattern evaluations do not sum");
        return true;
    } catch (const std::exception &exception) {
        return failWith(error, exception.what());
    }
}
