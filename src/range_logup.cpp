#include "range_logup.hpp"

#include "hyrax_opening.hpp"
#include "hyrax_rp.hpp"
#include "range_sumcheck.hpp"
#include "thread_joiner.hpp"

#include <algorithm>
#include <atomic>
#include <exception>
#include <stdexcept>
#include <thread>

namespace {

bool failWith(std::string *error, const std::string &message) {
    if (error) *error=message;
    return false;
}

bool isPowerOfTwo(std::size_t value) {
    return value!=0 && (value&(value-1))==0;
}

void invertNonZeroBatch(const std::vector<Fr> &input,
                        std::vector<Fr> &output,
                        int requested_threads) {
    if (input.size()!=output.size())
        throw std::invalid_argument("batch inversion size mismatch");
    if (input.empty()) return;

    // MCL's invVec replaces one field inversion per element with one inversion
    // per batch plus field multiplications.  Large LogUp value vectors are the
    // dominant Range-Proof input, so split independent batches across workers.
    constexpr std::size_t kMinimumValuesPerWorker=1ULL<<16;
    const std::size_t useful_workers=
        (input.size()+kMinimumValuesPerWorker-1)/kMinimumValuesPerWorker;
    const std::size_t worker_count=std::max<std::size_t>(
        1,std::min<std::size_t>(
            requested_threads>0 ? static_cast<std::size_t>(requested_threads) : 1,
            useful_workers));

    auto invert_range=[&](std::size_t worker) {
        const std::size_t begin=input.size()*worker/worker_count;
        const std::size_t end=input.size()*(worker+1)/worker_count;
        // A larger MCL work batch amortizes inversions while keeping the
        // temporary stack allocation modest (4096 Fr elements).
        mcl::invVec(output.data()+begin,input.data()+begin,end-begin,4096);
    };

    if (worker_count==1) {
        invert_range(0);
        return;
    }
    std::vector<std::exception_ptr> errors(worker_count);
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    zkgpt::ThreadJoiner joiner(workers);
    for (std::size_t worker=0;worker<worker_count;++worker)
        workers.emplace_back([&,worker] {
            try {
                invert_range(worker);
            } catch (...) {
                errors[worker]=std::current_exception();
            }
        });
    joiner.join();
    for (const auto &error : errors)
        if (error) std::rethrow_exception(error);
}

std::size_t exactLog2(std::size_t value) {
    if (!isPowerOfTwo(value))
        throw std::invalid_argument("LogUp vector size is not a power of two");
    std::size_t result=0;
    while (value>1) {
        value>>=1;
        ++result;
    }
    return result;
}

std::vector<Fr> derivePoint(Transcript &transcript,
                            const std::string &label,
                            std::size_t size) {
    std::vector<Fr> point;
    point.reserve(size);
    for (std::size_t i=0;i<size;++i)
        point.push_back(transcript.challenge(label));
    return point;
}

std::vector<Fr> eqWeights(const std::vector<Fr> &point) {
    std::vector<Fr> weights(static_cast<std::size_t>(1)<<point.size(), Fr(0));
    weights[0]=1;
    std::size_t active=1;
    for (const Fr &challenge : point) {
        for (std::size_t i=active;i>0;--i) {
            const Fr previous=weights[i-1];
            weights[i-1]=previous*(Fr(1)-challenge);
            weights[i-1+active]=previous*challenge;
        }
        active*=2;
    }
    return weights;
}

Fr evaluateMle(const std::vector<Fr> &values,
               const std::vector<Fr> &point) {
    const auto weights=eqWeights(point);
    if (weights.size()!=values.size())
        throw std::invalid_argument("LogUp MLE evaluation shape mismatch");
    Fr result=0;
    for (std::size_t i=0;i<values.size();++i) result+=weights[i]*values[i];
    return result;
}

std::vector<G1> copyCommitments(G1 *commitments, std::size_t count) {
    std::vector<G1> result(commitments, commitments+count);
    delete[] commitments;
    return result;
}

std::vector<G1> commitIntegerValuesImpl(
    const std::vector<ll> &values, const std::vector<G1> &generators,
    int threads) {
    const std::size_t log_size=exactLog2(values.size());
    const std::size_t rows=static_cast<std::size_t>(1)<<(log_size/2);
    const std::size_t columns=values.size()/rows;
    if (generators.size()<columns)
        throw std::invalid_argument("LogUp integer generator count mismatch");
    ll maximum=0;
    for (const ll value : values) {
        if (value<0)
            throw std::invalid_argument("LogUp integer commitment is negative");
        maximum=std::max(maximum,value);
    }

    // Large LogUp chunk rows contain many 9-bit scalars.  Group equal scalars
    // and evaluate sum_s s*bucket[s] with the descending-bucket identity.
    // Small tables and multiplicity vectors use ordinary MSM below; neither
    // path allocates the legacy fixed 2^18-bucket scratch area.
    constexpr ll kMaximumGroupedScalar=(1<<12)-1;
    constexpr std::size_t kMinimumGroupedColumns=256;
    if (columns>=kMinimumGroupedColumns && maximum<=kMaximumGroupedScalar) {
        const std::size_t bucket_count=static_cast<std::size_t>(maximum)+1;
        std::vector<G1> commitments(rows);
        const std::size_t worker_count=std::min<std::size_t>(
            rows,std::max(1,threads));
        std::atomic<std::size_t> next_row{0};
        std::atomic<bool> failed{false};
        std::vector<std::exception_ptr> errors(worker_count);
        std::vector<std::thread> workers;
        workers.reserve(worker_count);
        zkgpt::ThreadJoiner joiner(workers);
        for (std::size_t worker=0;worker<worker_count;++worker) {
            workers.emplace_back([&,worker] {
                try {
                    std::vector<G1> buckets(bucket_count);
                    while (!failed.load(std::memory_order_relaxed)) {
                        const std::size_t row=next_row.fetch_add(1);
                        if (row>=rows) break;
                        for (auto &bucket : buckets) bucket.clear();
                        for (std::size_t column=0;column<columns;++column) {
                            const auto scalar=static_cast<std::size_t>(
                                values[row+column*rows]);
                            if (scalar!=0)
                                buckets[scalar]+=generators[column];
                        }
                        G1 running;
                        G1 commitment;
                        running.clear();
                        commitment.clear();
                        for (std::size_t scalar=bucket_count;scalar>1;--scalar) {
                            running+=buckets[scalar-1];
                            commitment+=running;
                        }
                        commitments[row]=commitment;
                    }
                } catch (...) {
                    failed.store(true,std::memory_order_relaxed);
                    errors[worker]=std::current_exception();
                }
            });
        }
        joiner.join();
        for (const auto &error : errors)
            if (error) std::rethrow_exception(error);
        return commitments;
    }

    std::vector<Fr> field_values(values.size());
    for (std::size_t i=0;i<values.size();++i) field_values[i]=Fr(values[i]);
    return copyCommitments(range_proof_prover_commit_fr_general(
        field_values.data(),const_cast<G1 *>(generators.data()),log_size,
        std::max(1,threads)),rows);
}

std::vector<G1> commitFieldValues(const std::vector<Fr> &values,
                                  const std::vector<G1> &generators,
                                  int threads) {
    const std::size_t log_size=exactLog2(values.size());
    const std::size_t rows=static_cast<std::size_t>(1)<<(log_size/2);
    const std::size_t columns=values.size()/rows;
    if (generators.size()<columns)
        throw std::invalid_argument("LogUp field generator count mismatch");
    return copyCommitments(range_proof_prover_commit_fr_general(
        const_cast<Fr *>(values.data()),
        const_cast<G1 *>(generators.data()),log_size,std::max(1,threads)),rows);
}

std::vector<G1> commitPublicTable(const std::vector<Fr> &values,
                                  const std::vector<G1> &generators) {
    const std::size_t log_size=exactLog2(values.size());
    const std::size_t row_count=static_cast<std::size_t>(1)<<(log_size/2);
    const std::size_t column_count=values.size()/row_count;
    if (generators.size()<column_count)
        throw std::invalid_argument("LogUp table generator count mismatch");
    std::vector<G1> commitments(row_count);
    std::vector<Fr> row_values(column_count);
    for (std::size_t row=0;row<row_count;++row) {
        for (std::size_t column=0;column<column_count;++column)
            row_values[column]=values[row+column*row_count];
        G1::mulVec(commitments[row],
                   const_cast<G1 *>(generators.data()), row_values.data(),
                   column_count);
    }
    return commitments;
}

void appendCommitmentVector(Transcript &transcript, const std::string &label,
                            const std::vector<G1> &commitments) {
    transcript.appendU64(label+".count", commitments.size());
    for (const auto &commitment : commitments)
        transcript.appendG1(label, commitment);
}

void appendLogUpHeader(Transcript &transcript, std::size_t query_index,
                       std::size_t chunk_index, unsigned chunk_bits,
                       std::size_t table_size) {
    transcript.appendU64("logup.query", query_index);
    transcript.appendU64("logup.chunk", chunk_index);
    transcript.appendU64("logup.chunk_bits", chunk_bits);
    transcript.appendU64("logup.table_size", table_size);
}

void appendInitialCommitments(Transcript &transcript, const LogUpProof &proof) {
    appendCommitmentVector(transcript, "logup.value_commitment",
                           proof.value_commitment);
    appendCommitmentVector(transcript, "logup.table_commitment",
                           proof.table_commitment);
    appendCommitmentVector(transcript, "logup.multiplicity_commitment",
                           proof.multiplicity_commitment);
}

void appendReciprocalCommitments(Transcript &transcript,
                                 const LogUpProof &proof) {
    appendCommitmentVector(transcript, "logup.reciprocal_value_commitment",
                           proof.reciprocal_value_commitment);
    appendCommitmentVector(transcript, "logup.reciprocal_table_commitment",
                           proof.reciprocal_table_commitment);
}

std::string openingLabel(std::size_t query_index, std::size_t chunk_index,
                         const std::string &name) {
    return "logup/"+std::to_string(query_index)+"/"+
           std::to_string(chunk_index)+"/"+name;
}

}  // namespace

namespace range_logup {

std::vector<G1> commitIntegerValues(
    const std::vector<ll> &values, const std::vector<G1> &generators,
    int threads) {
    return commitIntegerValuesImpl(values,generators,threads);
}

}  // namespace range_logup

LogUpProof proveLogUp(
    std::size_t query_index, std::size_t chunk_index,
    unsigned chunk_bits, const std::vector<std::uint16_t> &values,
    const std::vector<G1> &generators, const G1 &u,
    int thread_count) {
    if (!isPowerOfTwo(values.size()) || chunk_bits==0 || chunk_bits>9)
        throw std::invalid_argument("invalid LogUp value vector");
    const std::size_t table_size=static_cast<std::size_t>(1)<<chunk_bits;
    std::vector<ll> integer_values(values.begin(), values.end());
    std::vector<ll> integer_table(table_size);
    std::vector<ll> integer_multiplicity(table_size, 0);
    std::vector<Fr> value_fields(values.size());
    std::vector<Fr> table_fields(table_size);
    std::vector<Fr> multiplicity_fields(table_size);
    for (std::size_t i=0;i<values.size();++i) {
        if (values[i]>=table_size)
            throw std::range_error("LogUp value is outside the public table");
        value_fields[i]=Fr(values[i]);
        ++integer_multiplicity[values[i]];
    }
    for (std::size_t i=0;i<table_size;++i) {
        integer_table[i]=i;
        table_fields[i]=Fr(i);
        multiplicity_fields[i]=Fr(integer_multiplicity[i]);
    }

    LogUpProof proof;
    proof.query_index=query_index;
    proof.chunk_index=chunk_index;
    proof.chunk_bits=chunk_bits;
    proof.table_size=table_size;
    proof.value_commitment=range_logup::commitIntegerValues(
        integer_values,generators,thread_count);
    proof.table_commitment=range_logup::commitIntegerValues(
        integer_table,generators,thread_count);
    proof.multiplicity_commitment=range_logup::commitIntegerValues(
        integer_multiplicity,generators,thread_count);

    Transcript transcript("zkGPT-logup-v1");
    appendLogUpHeader(transcript, query_index, chunk_index, chunk_bits,
                      table_size);
    appendInitialCommitments(transcript, proof);
    const Fr offset=transcript.challenge("logup.offset");

    std::vector<Fr> shifted_values(values.size());
    std::vector<Fr> reciprocal_values(values.size());
    for (std::size_t i=0;i<values.size();++i) {
        shifted_values[i]=offset+value_fields[i];
        if (shifted_values[i].isZero())
            throw std::runtime_error("LogUp value denominator is zero");
    }
    std::vector<Fr> shifted_table(table_size);
    std::vector<Fr> reciprocal_table(table_size);
    for (std::size_t i=0;i<table_size;++i) {
        shifted_table[i]=offset+table_fields[i];
        if (shifted_table[i].isZero())
            throw std::runtime_error("LogUp table denominator is zero");
    }
    invertNonZeroBatch(shifted_values,reciprocal_values,thread_count);
    std::vector<Fr> inverse_table(table_size);
    invertNonZeroBatch(shifted_table,inverse_table,thread_count);
    for (std::size_t i=0;i<table_size;++i)
        reciprocal_table[i]=multiplicity_fields[i]*inverse_table[i];
    proof.reciprocal_value_commitment=commitFieldValues(
        reciprocal_values,generators,thread_count);
    proof.reciprocal_table_commitment=commitFieldValues(
        reciprocal_table,generators,thread_count);
    appendReciprocalCommitments(transcript, proof);

    const auto table_point=derivePoint(
        transcript, "logup.table_statement_point", exactLog2(table_size));
    const auto value_point=derivePoint(
        transcript, "logup.value_statement_point", exactLog2(values.size()));
    proof.multiplicity_evaluation=evaluateMle(
        multiplicity_fields, table_point);
    transcript.appendFr("logup.multiplicity_evaluation",
                        proof.multiplicity_evaluation);

    std::vector<Fr> table_sumcheck_point;
    proof.reciprocal_table_sumcheck=proveDegree3Sumcheck(
        table_point, reciprocal_table, shifted_table,
        proof.multiplicity_evaluation, transcript,
        "logup.table_reciprocal", &table_sumcheck_point);
    std::vector<Fr> value_sumcheck_point;
    proof.reciprocal_value_sumcheck=proveDegree3Sumcheck(
        value_point, reciprocal_values, shifted_values, Fr(1), transcript,
        "logup.value_reciprocal", &value_sumcheck_point);

    proof.reciprocal_sum=0;
    for (const Fr &value : reciprocal_values) proof.reciprocal_sum+=value;
    Fr table_reciprocal_sum=0;
    for (const Fr &value : reciprocal_table) table_reciprocal_sum+=value;
    if (proof.reciprocal_sum!=table_reciprocal_sum)
        throw std::logic_error("LogUp reciprocal sums disagree");
    transcript.appendFr("logup.reciprocal_sum", proof.reciprocal_sum);
    std::vector<Fr> value_equality_point;
    proof.reciprocal_value_equality=proveDegree1Sumcheck(
        reciprocal_values, proof.reciprocal_sum, transcript,
        "logup.value_sum", &value_equality_point);
    std::vector<Fr> table_equality_point;
    proof.reciprocal_table_equality=proveDegree1Sumcheck(
        reciprocal_table, proof.reciprocal_sum, transcript,
        "logup.table_sum", &table_equality_point);

    proof.multiplicity_opening=hyraxMleOpenProveFr(
        multiplicity_fields, proof.multiplicity_commitment, table_point,
        generators, u, proof.multiplicity_evaluation, transcript,
        openingLabel(query_index, chunk_index, "multiplicity"));
    proof.reciprocal_table_opening=hyraxMleOpenProveFr(
        reciprocal_table, proof.reciprocal_table_commitment,
        table_sumcheck_point, generators, u,
        proof.reciprocal_table_sumcheck.final_f_evaluation, transcript,
        openingLabel(query_index, chunk_index, "table-reciprocal"));
    proof.reciprocal_value_opening=hyraxMleOpenProveFr(
        reciprocal_values, proof.reciprocal_value_commitment,
        value_sumcheck_point, generators, u,
        proof.reciprocal_value_sumcheck.final_f_evaluation, transcript,
        openingLabel(query_index, chunk_index, "value-reciprocal"));
    proof.value_opening=hyraxMleOpenProve(
        values, proof.value_commitment, value_sumcheck_point,
        generators, u,
        proof.reciprocal_value_sumcheck.final_g_evaluation-offset,
        transcript, openingLabel(query_index, chunk_index, "value"));
    proof.reciprocal_value_sum_opening=hyraxMleOpenProveFr(
        reciprocal_values, proof.reciprocal_value_commitment,
        value_equality_point, generators, u,
        proof.reciprocal_value_equality.final_evaluation, transcript,
        openingLabel(query_index, chunk_index, "value-sum"));
    proof.reciprocal_table_sum_opening=hyraxMleOpenProveFr(
        reciprocal_table, proof.reciprocal_table_commitment,
        table_equality_point, generators, u,
        proof.reciprocal_table_equality.final_evaluation, transcript,
        openingLabel(query_index, chunk_index, "table-sum"));
    proof.transcript_binding=transcript.challenge("logup.final");
    return proof;
}

bool verifyLogUp(
    const PublicRangeQuery &query, std::size_t query_index,
    std::size_t chunk_index, const std::vector<G1> &chunk_commitment,
    const LogUpProof &proof, const std::vector<G1> &generators,
    const G1 &u, std::string *error) {
    try {
        if (chunk_index>=query.chunk_bits.size())
            return failWith(error, "LogUp chunk index is invalid");
        const unsigned chunk_bits=query.chunk_bits[chunk_index];
        if (chunk_bits==0 || chunk_bits>9)
            return failWith(error, "LogUp chunk width is invalid");
        const std::size_t table_size=static_cast<std::size_t>(1)<<chunk_bits;
        if (proof.query_index!=query_index ||
            proof.chunk_index!=chunk_index ||
            proof.chunk_bits!=chunk_bits || proof.table_size!=table_size)
            return failWith(error, "LogUp public metadata mismatch");
        if (proof.value_commitment!=chunk_commitment)
            return failWith(error, "LogUp value commitment mismatch");
        const std::size_t value_rounds=exactLog2(query.padded_query_size);
        const std::size_t table_rounds=exactLog2(table_size);
        if (proof.reciprocal_value_sumcheck.rounds.size()!=value_rounds ||
            proof.reciprocal_value_equality.rounds.size()!=value_rounds ||
            proof.reciprocal_table_sumcheck.rounds.size()!=table_rounds ||
            proof.reciprocal_table_equality.rounds.size()!=table_rounds)
            return failWith(error, "LogUp sumcheck round count mismatch");
        std::vector<Fr> table_fields(table_size);
        for (std::size_t i=0;i<table_size;++i) table_fields[i]=Fr(i);
        if (proof.table_commitment!=commitPublicTable(table_fields, generators))
            return failWith(error, "LogUp public table commitment mismatch");

        Transcript transcript("zkGPT-logup-v1");
        appendLogUpHeader(transcript, query_index, chunk_index, chunk_bits,
                          table_size);
        appendInitialCommitments(transcript, proof);
        const Fr offset=transcript.challenge("logup.offset");
        appendReciprocalCommitments(transcript, proof);
        const auto table_point=derivePoint(
            transcript, "logup.table_statement_point", exactLog2(table_size));
        const auto value_point=derivePoint(
            transcript, "logup.value_statement_point",
            exactLog2(query.padded_query_size));
        transcript.appendFr("logup.multiplicity_evaluation",
                            proof.multiplicity_evaluation);

        std::vector<Fr> table_sumcheck_point;
        std::string sumcheck_error;
        if (!verifyDegree3Sumcheck(
                table_point, proof.multiplicity_evaluation,
                proof.reciprocal_table_sumcheck, transcript,
                "logup.table_reciprocal", &table_sumcheck_point,
                &sumcheck_error))
            return failWith(error, "LogUp table reciprocal sumcheck failed: "+
                            sumcheck_error);
        std::vector<Fr> value_sumcheck_point;
        if (!verifyDegree3Sumcheck(
                value_point, Fr(1), proof.reciprocal_value_sumcheck,
                transcript, "logup.value_reciprocal",
                &value_sumcheck_point, &sumcheck_error))
            return failWith(error, "LogUp value reciprocal sumcheck failed: "+
                            sumcheck_error);
        transcript.appendFr("logup.reciprocal_sum", proof.reciprocal_sum);
        std::vector<Fr> value_equality_point;
        if (!verifyDegree1Sumcheck(
                proof.reciprocal_sum, proof.reciprocal_value_equality,
                transcript, "logup.value_sum", &value_equality_point,
                &sumcheck_error))
            return failWith(error, "LogUp value sumcheck failed: "+
                            sumcheck_error);
        std::vector<Fr> table_equality_point;
        if (!verifyDegree1Sumcheck(
                proof.reciprocal_sum, proof.reciprocal_table_equality,
                transcript, "logup.table_sum", &table_equality_point,
                &sumcheck_error))
            return failWith(error, "LogUp table sumcheck failed: "+
                            sumcheck_error);

        std::string opening_error;
        if (!hyraxMleOpenVerify(
                proof.multiplicity_commitment, table_point, generators, u,
                proof.multiplicity_evaluation, proof.multiplicity_opening,
                transcript, openingLabel(query_index, chunk_index,
                                         "multiplicity"), &opening_error))
            return failWith(error, "LogUp multiplicity opening failed: "+
                            opening_error);
        if (!hyraxMleOpenVerify(
                proof.reciprocal_table_commitment, table_sumcheck_point,
                generators, u,
                proof.reciprocal_table_sumcheck.final_f_evaluation,
                proof.reciprocal_table_opening, transcript,
                openingLabel(query_index, chunk_index, "table-reciprocal"),
                &opening_error))
            return failWith(error, "LogUp table reciprocal opening failed: "+
                            opening_error);
        if (!hyraxMleOpenVerify(
                proof.reciprocal_value_commitment, value_sumcheck_point,
                generators, u,
                proof.reciprocal_value_sumcheck.final_f_evaluation,
                proof.reciprocal_value_opening, transcript,
                openingLabel(query_index, chunk_index, "value-reciprocal"),
                &opening_error))
            return failWith(error, "LogUp value reciprocal opening failed: "+
                            opening_error);
        const Fr table_evaluation=evaluateMle(table_fields,
                                              table_sumcheck_point);
        if (proof.reciprocal_table_sumcheck.final_g_evaluation!=
                table_evaluation+offset)
            return failWith(error, "LogUp shifted table evaluation mismatch");
        const Fr value_evaluation=
            proof.reciprocal_value_sumcheck.final_g_evaluation-offset;
        if (!hyraxMleOpenVerify(
                proof.value_commitment, value_sumcheck_point, generators, u,
                value_evaluation, proof.value_opening, transcript,
                openingLabel(query_index, chunk_index, "value"),
                &opening_error))
            return failWith(error, "LogUp value opening failed: "+opening_error);
        if (!hyraxMleOpenVerify(
                proof.reciprocal_value_commitment, value_equality_point,
                generators, u,
                proof.reciprocal_value_equality.final_evaluation,
                proof.reciprocal_value_sum_opening, transcript,
                openingLabel(query_index, chunk_index, "value-sum"),
                &opening_error))
            return failWith(error, "LogUp value sum opening failed: "+
                            opening_error);
        if (!hyraxMleOpenVerify(
                proof.reciprocal_table_commitment, table_equality_point,
                generators, u,
                proof.reciprocal_table_equality.final_evaluation,
                proof.reciprocal_table_sum_opening, transcript,
                openingLabel(query_index, chunk_index, "table-sum"),
                &opening_error))
            return failWith(error, "LogUp table sum opening failed: "+
                            opening_error);
        if (transcript.challenge("logup.final")!=proof.transcript_binding)
            return failWith(error, "LogUp transcript binding mismatch");
        return true;
    } catch (const std::exception &exception) {
        return failWith(error, exception.what());
    }
}

LogUpProof proveFieldLogUp(
    std::size_t query_index, const std::vector<Fr> &value_fields,
    const std::vector<Fr> &table_fields, const std::vector<ll> &counts,
    const std::vector<G1> &generators, const G1 &u, int thread_count,
    const Fr &context, bool adversarial_skip_sum_check) {
    if (!isPowerOfTwo(value_fields.size()) || !isPowerOfTwo(table_fields.size()) || counts.size()!=table_fields.size())
        throw std::invalid_argument("field LogUp dimensions");
    const std::size_t table_size=table_fields.size(), chunk_index=0;
    const unsigned chunk_bits=0;
    std::vector<Fr> multiplicity_fields(table_size);
    for (size_t i=0;i<table_size;i++) multiplicity_fields[i]=Fr(counts[i]);
    LogUpProof proof;
    proof.query_index=query_index;
    proof.chunk_index=chunk_index;
    proof.chunk_bits=chunk_bits;
    proof.table_size=table_size;
    proof.value_commitment=commitFieldValues(value_fields,generators,thread_count);
    proof.table_commitment=commitPublicTable(table_fields,generators);
    proof.multiplicity_commitment=range_logup::commitIntegerValues(
        counts,generators,thread_count);

    Transcript transcript("zkGPT-exp-field-logup-v4");
    transcript.appendFr("context",context);
    appendLogUpHeader(transcript, query_index, chunk_index, chunk_bits,
                      table_size);
    appendInitialCommitments(transcript, proof);
    const Fr offset=transcript.challenge("logup.offset");

    std::vector<Fr> shifted_values(value_fields.size());
    std::vector<Fr> reciprocal_values(value_fields.size());
    for (std::size_t i=0;i<value_fields.size();++i) {
        shifted_values[i]=offset+value_fields[i];
        if (shifted_values[i].isZero())
            throw std::runtime_error("LogUp value denominator is zero");
    }
    std::vector<Fr> shifted_table(table_size);
    std::vector<Fr> reciprocal_table(table_size);
    for (std::size_t i=0;i<table_size;++i) {
        shifted_table[i]=offset+table_fields[i];
        if (shifted_table[i].isZero())
            throw std::runtime_error("LogUp table denominator is zero");
    }
    invertNonZeroBatch(shifted_values,reciprocal_values,thread_count);
    std::vector<Fr> inverse_table(table_size);
    invertNonZeroBatch(shifted_table,inverse_table,thread_count);
    for (std::size_t i=0;i<table_size;++i)
        reciprocal_table[i]=multiplicity_fields[i]*inverse_table[i];
    proof.reciprocal_value_commitment=commitFieldValues(
        reciprocal_values,generators,thread_count);
    proof.reciprocal_table_commitment=commitFieldValues(
        reciprocal_table,generators,thread_count);
    appendReciprocalCommitments(transcript, proof);

    const auto table_point=derivePoint(
        transcript, "logup.table_statement_point", exactLog2(table_size));
    const auto value_point=derivePoint(
        transcript, "logup.value_statement_point", exactLog2(value_fields.size()));
    proof.multiplicity_evaluation=evaluateMle(
        multiplicity_fields, table_point);
    transcript.appendFr("logup.multiplicity_evaluation",
                        proof.multiplicity_evaluation);

    std::vector<Fr> table_sumcheck_point;
    proof.reciprocal_table_sumcheck=proveDegree3Sumcheck(
        table_point, reciprocal_table, shifted_table,
        proof.multiplicity_evaluation, transcript,
        "logup.table_reciprocal", &table_sumcheck_point);
    std::vector<Fr> value_sumcheck_point;
    proof.reciprocal_value_sumcheck=proveDegree3Sumcheck(
        value_point, reciprocal_values, shifted_values, Fr(1), transcript,
        "logup.value_reciprocal", &value_sumcheck_point);

    proof.reciprocal_sum=0;
    for (const Fr &value : reciprocal_values) proof.reciprocal_sum+=value;
    Fr table_reciprocal_sum=0;
    for (const Fr &value : reciprocal_table) table_reciprocal_sum+=value;
    if (!adversarial_skip_sum_check && proof.reciprocal_sum!=table_reciprocal_sum)
        throw std::logic_error("LogUp reciprocal sums disagree");
    transcript.appendFr("logup.reciprocal_sum", proof.reciprocal_sum);
    std::vector<Fr> value_equality_point;
    proof.reciprocal_value_equality=proveDegree1Sumcheck(
        reciprocal_values, proof.reciprocal_sum, transcript,
        "logup.value_sum", &value_equality_point);
    std::vector<Fr> table_equality_point;
    proof.reciprocal_table_equality=proveDegree1Sumcheck(
        reciprocal_table, adversarial_skip_sum_check?table_reciprocal_sum:proof.reciprocal_sum, transcript,
        "logup.table_sum", &table_equality_point);

    proof.multiplicity_opening=hyraxMleOpenProveFr(
        multiplicity_fields, proof.multiplicity_commitment, table_point,
        generators, u, proof.multiplicity_evaluation, transcript,
        openingLabel(query_index, chunk_index, "multiplicity"));
    proof.reciprocal_table_opening=hyraxMleOpenProveFr(
        reciprocal_table, proof.reciprocal_table_commitment,
        table_sumcheck_point, generators, u,
        proof.reciprocal_table_sumcheck.final_f_evaluation, transcript,
        openingLabel(query_index, chunk_index, "table-reciprocal"));
    proof.reciprocal_value_opening=hyraxMleOpenProveFr(
        reciprocal_values, proof.reciprocal_value_commitment,
        value_sumcheck_point, generators, u,
        proof.reciprocal_value_sumcheck.final_f_evaluation, transcript,
        openingLabel(query_index, chunk_index, "value-reciprocal"));
    proof.value_opening=hyraxMleOpenProveFr(
        value_fields, proof.value_commitment, value_sumcheck_point,
        generators, u,
        proof.reciprocal_value_sumcheck.final_g_evaluation-offset,
        transcript, openingLabel(query_index, chunk_index, "value"));
    proof.reciprocal_value_sum_opening=hyraxMleOpenProveFr(
        reciprocal_values, proof.reciprocal_value_commitment,
        value_equality_point, generators, u,
        proof.reciprocal_value_equality.final_evaluation, transcript,
        openingLabel(query_index, chunk_index, "value-sum"));
    proof.reciprocal_table_sum_opening=hyraxMleOpenProveFr(
        reciprocal_table, proof.reciprocal_table_commitment,
        table_equality_point, generators, u,
        proof.reciprocal_table_equality.final_evaluation, transcript,
        openingLabel(query_index, chunk_index, "table-sum"));
    proof.transcript_binding=transcript.challenge("logup.final");
    return proof;
}


std::vector<G1> commitLookupFields(const std::vector<Fr> &v,const std::vector<G1> &g,int threads) { return commitFieldValues(v,g,threads); }

bool verifyFieldLogUp(
    std::size_t query_size, std::size_t query_index,
    const std::vector<Fr> &table_fields, const std::vector<G1> &chunk_commitment,
    const LogUpProof &proof, const std::vector<G1> &generators,
    const G1 &u, const Fr &context, std::string *error) {
    try {
        const std::size_t chunk_index=0, table_size=table_fields.size();
        const unsigned chunk_bits=0;
        if (proof.query_index!=query_index ||
            proof.chunk_index!=chunk_index ||
            proof.chunk_bits!=chunk_bits || proof.table_size!=table_size)
            return failWith(error, "LogUp public metadata mismatch");
        if (proof.value_commitment!=chunk_commitment)
            return failWith(error, "LogUp value commitment mismatch");
        const std::size_t value_rounds=exactLog2(query_size);
        const std::size_t table_rounds=exactLog2(table_size);
        if (proof.reciprocal_value_sumcheck.rounds.size()!=value_rounds ||
            proof.reciprocal_value_equality.rounds.size()!=value_rounds ||
            proof.reciprocal_table_sumcheck.rounds.size()!=table_rounds ||
            proof.reciprocal_table_equality.rounds.size()!=table_rounds)
            return failWith(error, "LogUp sumcheck round count mismatch");
        if (proof.table_commitment!=commitPublicTable(table_fields, generators))
            return failWith(error, "LogUp public table commitment mismatch");

        Transcript transcript("zkGPT-exp-field-logup-v4");
        transcript.appendFr("context",context);
        appendLogUpHeader(transcript, query_index, chunk_index, chunk_bits,
                          table_size);
        appendInitialCommitments(transcript, proof);
        const Fr offset=transcript.challenge("logup.offset");
        for (const auto &v:table_fields) if ((offset+v).isZero()) return failWith(error,"zero public table denominator");
        appendReciprocalCommitments(transcript, proof);
        const auto table_point=derivePoint(
            transcript, "logup.table_statement_point", exactLog2(table_size));
        const auto value_point=derivePoint(
            transcript, "logup.value_statement_point",
            exactLog2(query_size));
        transcript.appendFr("logup.multiplicity_evaluation",
                            proof.multiplicity_evaluation);

        std::vector<Fr> table_sumcheck_point;
        std::string sumcheck_error;
        if (!verifyDegree3Sumcheck(
                table_point, proof.multiplicity_evaluation,
                proof.reciprocal_table_sumcheck, transcript,
                "logup.table_reciprocal", &table_sumcheck_point,
                &sumcheck_error))
            return failWith(error, "LogUp table reciprocal sumcheck failed: "+
                            sumcheck_error);
        std::vector<Fr> value_sumcheck_point;
        if (!verifyDegree3Sumcheck(
                value_point, Fr(1), proof.reciprocal_value_sumcheck,
                transcript, "logup.value_reciprocal",
                &value_sumcheck_point, &sumcheck_error))
            return failWith(error, "LogUp value reciprocal sumcheck failed: "+
                            sumcheck_error);
        transcript.appendFr("logup.reciprocal_sum", proof.reciprocal_sum);
        std::vector<Fr> value_equality_point;
        if (!verifyDegree1Sumcheck(
                proof.reciprocal_sum, proof.reciprocal_value_equality,
                transcript, "logup.value_sum", &value_equality_point,
                &sumcheck_error))
            return failWith(error, "LogUp value sumcheck failed: "+
                            sumcheck_error);
        std::vector<Fr> table_equality_point;
        if (!verifyDegree1Sumcheck(
                proof.reciprocal_sum, proof.reciprocal_table_equality,
                transcript, "logup.table_sum", &table_equality_point,
                &sumcheck_error))
            return failWith(error, "LogUp table sumcheck failed: "+
                            sumcheck_error);

        std::string opening_error;
        if (!hyraxMleOpenVerify(
                proof.multiplicity_commitment, table_point, generators, u,
                proof.multiplicity_evaluation, proof.multiplicity_opening,
                transcript, openingLabel(query_index, chunk_index,
                                         "multiplicity"), &opening_error))
            return failWith(error, "LogUp multiplicity opening failed: "+
                            opening_error);
        if (!hyraxMleOpenVerify(
                proof.reciprocal_table_commitment, table_sumcheck_point,
                generators, u,
                proof.reciprocal_table_sumcheck.final_f_evaluation,
                proof.reciprocal_table_opening, transcript,
                openingLabel(query_index, chunk_index, "table-reciprocal"),
                &opening_error))
            return failWith(error, "LogUp table reciprocal opening failed: "+
                            opening_error);
        if (!hyraxMleOpenVerify(
                proof.reciprocal_value_commitment, value_sumcheck_point,
                generators, u,
                proof.reciprocal_value_sumcheck.final_f_evaluation,
                proof.reciprocal_value_opening, transcript,
                openingLabel(query_index, chunk_index, "value-reciprocal"),
                &opening_error))
            return failWith(error, "LogUp value reciprocal opening failed: "+
                            opening_error);
        const Fr table_evaluation=evaluateMle(table_fields,
                                              table_sumcheck_point);
        if (proof.reciprocal_table_sumcheck.final_g_evaluation!=
                table_evaluation+offset)
            return failWith(error, "LogUp shifted table evaluation mismatch");
        const Fr value_evaluation=
            proof.reciprocal_value_sumcheck.final_g_evaluation-offset;
        if (!hyraxMleOpenVerify(
                proof.value_commitment, value_sumcheck_point, generators, u,
                value_evaluation, proof.value_opening, transcript,
                openingLabel(query_index, chunk_index, "value"),
                &opening_error))
            return failWith(error, "LogUp value opening failed: "+opening_error);
        if (!hyraxMleOpenVerify(
                proof.reciprocal_value_commitment, value_equality_point,
                generators, u,
                proof.reciprocal_value_equality.final_evaluation,
                proof.reciprocal_value_sum_opening, transcript,
                openingLabel(query_index, chunk_index, "value-sum"),
                &opening_error))
            return failWith(error, "LogUp value sum opening failed: "+
                            opening_error);
        if (!hyraxMleOpenVerify(
                proof.reciprocal_table_commitment, table_equality_point,
                generators, u,
                proof.reciprocal_table_equality.final_evaluation,
                proof.reciprocal_table_sum_opening, transcript,
                openingLabel(query_index, chunk_index, "table-sum"),
                &opening_error))
            return failWith(error, "LogUp table sum opening failed: "+
                            opening_error);
        if (transcript.challenge("logup.final")!=proof.transcript_binding)
            return failWith(error, "LogUp transcript binding mismatch");
        return true;
    } catch (const std::exception &exception) {
        return failWith(error, exception.what());
    }
}
