#pragma once

#include "range_protocol.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace range_logup {

// Commit a nonnegative, power-of-two integer vector in the same column-major
// Hyrax layout used by the LogUp prover.  Kept as a kernel-level API so its
// optimized path can be checked directly against an ordinary MSM.
std::vector<G1> commitIntegerValues(
    const std::vector<ll> &values, const std::vector<G1> &generators,
    int threads);

}  // namespace range_logup

LogUpProof proveLogUp(
    std::size_t query_index, std::size_t chunk_index,
    unsigned chunk_bits, const std::vector<std::uint16_t> &values,
    const std::vector<G1> &generators, const G1 &u,
    int thread_count);

bool verifyLogUp(
    const PublicRangeQuery &query, std::size_t query_index,
    std::size_t chunk_index, const std::vector<G1> &chunk_commitment,
    const LogUpProof &proof, const std::vector<G1> &generators,
    const G1 &u, std::string *error = nullptr);

// Fixed public arbitrary Fr table. Counts are committed before the pole
// challenge; the caller binds query commitments and the context to its statement.
LogUpProof proveFieldLogUp(std::size_t query_index,
    const std::vector<Fr> &values, const std::vector<Fr> &table,
    const std::vector<ll> &counts, const std::vector<G1> &generators,
    const G1 &u, int threads, const Fr &context,
    bool adversarial_skip_sum_check = false);
bool verifyFieldLogUp(std::size_t query_size, std::size_t query_index,
    const std::vector<Fr> &table, const std::vector<G1> &query_commitment,
    const LogUpProof &, const std::vector<G1> &generators, const G1 &u,
    const Fr &context, std::string *error = nullptr);
std::vector<G1> commitLookupFields(const std::vector<Fr> &,
    const std::vector<G1> &, int threads);

#include "zk_hyrax.hpp"
#include "range_sumcheck.hpp"
namespace zkgpt::zk {
// Membership covers every padded entry. The application separately constrains
// the padding coordinates and binds this exact commitment to its parent witness.
struct FieldLogUpZkStatement {
    std::size_t query_index=0, actual_query_size=0;
    MatrixCommitment query;
    GeneratorDomain generator_domain;
    std::vector<Fr> table;
    std::string table_id, context;
    std::array<std::uint8_t,32> circuit_id{};
    std::uint64_t layer_id=0;
    Fr padding_value=Fr(0);
};
struct FieldLogUpZkProof {
    MatrixCommitment Cm,Ca,Cb;
    G1 D_M,D_S;
    Degree3ZkProof table_identity,query_identity;
    Degree1ZkProof query_sum,table_sum;
    EqualityProof public_table_factor;
    // m,b,a,q,a-sum,b-sum, in this order.
    std::array<LinearProof,6> openings;
    Fr binding=Fr(0);
};
void validateFieldLogUpZkStatement(const FieldLogUpZkStatement&);
FieldLogUpZkProof proveFieldLogUpZk(const FieldLogUpZkStatement&,MatrixWitnessView,
    const std::vector<ll>&,int,ChallengeObserver* =nullptr);
bool verifyFieldLogUpZk(const FieldLogUpZkStatement&,const FieldLogUpZkProof&,
    std::string* =nullptr,ChallengeObserver* =nullptr);
Bytes encodeFieldLogUpZkStatement(const FieldLogUpZkStatement&);
FieldLogUpZkStatement decodeFieldLogUpZkStatement(const Bytes&);
Bytes encodeFieldLogUpZkProof(const FieldLogUpZkStatement&,const FieldLogUpZkProof&);
FieldLogUpZkProof decodeFieldLogUpZkProof(const FieldLogUpZkStatement&,const Bytes&);
}
