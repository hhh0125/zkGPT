#pragma once
#include "generator_cache.hpp"
#include "range_protocol.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>

// Production foundation API. This is not an end-to-end ZK protocol.
namespace zkgpt::zk {
using Bytes = std::vector<std::uint8_t>;
inline constexpr const char *kGeneratorDomain =
    "zkGPT/production/zk-foundation/BN254-mcl-default-map/v1";
inline constexpr std::size_t kMaxColumns = 32768, kMaxBatch = 65536, kMaxContextBytes = 4096;
struct Parameters {
    std::string domain = kGeneratorDomain;
    std::string vector_domain = std::string(kGeneratorDomain) + "/vector";
    std::vector<G1> G;
    G1 U, H;
};
struct ScalarWitness {
    Fr value = Fr(0), blinder = Fr(0);
};
Parameters makeParameters(std::size_t columns);
// Keeps the established deterministic vector basis; scalar U/H are global.
Parameters makeProductionParameters(const GeneratorDomain &);
void validateParameters(const Parameters &);
bool validPoint(const G1 &);
void requirePoint(const G1 &);
void requireScalar(const Fr &);
bool scalarCandidate(const std::array<std::uint8_t, 32> &, Fr &);
Fr sampleScalar();
G1 scalarCommit(const Parameters &, const ScalarWitness &);
// Binds foundation suite, caller's label and context to an existing transcript.
void bindContext(Transcript &, const Parameters &, const std::string &protocol,
                 const std::string &label, const std::string &context);
struct RepresentationProof {
    std::vector<G1> A;
    std::vector<Fr> z_value, z_blind;
};
struct EqualityProof {
    G1 A;
    Fr z = Fr(0);
};
struct ProductProof {
    G1 A, B;
    Fr sx = Fr(0), sr = Fr(0), st = Fr(0);
};
// Use the batch representation proof before the masking rho challenge. Shared
// challenge, individually checked equations; no random linear aggregation.
RepresentationProof proveRepresentation(const Parameters &, const std::vector<G1> &,
                                        const std::vector<ScalarWitness> &, Transcript &,
                                        const std::string &label, const std::string &context);
bool verifyRepresentation(const Parameters &, const std::vector<G1> &, const RepresentationProof &,
                          Transcript &, const std::string &label, const std::string &context,
                          std::string *error = nullptr);
EqualityProof proveEquality(const Parameters &, const std::vector<G1> &,
                            const std::vector<Fr> &alpha, const Fr &public_value,
                            const std::vector<ScalarWitness> &, Transcript &,
                            const std::string &label, const std::string &context);
bool verifyEquality(const Parameters &, const std::vector<G1> &, const std::vector<Fr> &alpha,
                    const Fr &public_value, const EqualityProof &, Transcript &,
                    const std::string &label, const std::string &context,
                    std::string *error = nullptr);
// Composition obligation: Cy's opening knowledge must come from a preceding
// representation PoK, extracted hidden opening, or a public constant. This
// proof alone establishes Cz=x*Cy+delta*H, not knowledge of Cy's value.
ProductProof proveProduct(const Parameters &, const G1 &Cx, const G1 &Cy, const G1 &Cz,
                          const ScalarWitness &x, const ScalarWitness &y, const ScalarWitness &z,
                          Transcript &, const std::string &label, const std::string &context);
bool verifyProduct(const Parameters &, const G1 &Cx, const G1 &Cy, const G1 &Cz,
                   const ProductProof &, Transcript &, const std::string &label,
                   const std::string &context, std::string *error = nullptr);
// Bounded canonical building blocks for the later protocol codec. Limits are
// caller-derived from public dimensions before any vector allocation.
class Writer {
  public:
    explicit Writer(std::size_t limit) : limit_(limit) {}
    void u64(std::uint64_t);
    void fr(const Fr &);
    void point(const G1 &);
    void bytes(const Bytes &);
    Bytes finish() const { return data_; }

  private:
    void raw(const std::uint8_t *, std::size_t);
    Bytes data_;
    std::size_t limit_;
};
class Reader {
  public:
    Reader(const Bytes &, std::size_t exact_size);
    std::uint64_t u64();
    Fr fr();
    G1 point();
    Bytes bytes(std::size_t count);
    void finish() const;

  private:
    const Bytes &data_;
    std::size_t pos_ = 0;
    void need(std::size_t) const;
};
Bytes encode(const RepresentationProof &);
RepresentationProof decodeRepresentation(const Bytes &, std::size_t expected_count);
Bytes encode(const EqualityProof &);
EqualityProof decodeEquality(const Bytes &);
Bytes encode(const ProductProof &);
ProductProof decodeProduct(const Bytes &);
} // namespace zkgpt::zk
