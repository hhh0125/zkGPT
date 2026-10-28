#pragma once
#include "crypto.hpp"
#include "paper_r1/pc.hpp"
namespace zkgpt::r7::hyrax_a3 {
using namespace crypto;
inline constexpr const char* suite="R7-MCL-BN254-HYRAX-A3-INTERACTIVE-v1";
inline constexpr const char* messageVersion="R7-HYRAX-A3-PROOF-v1";
inline constexpr std::size_t maxDot=1024;
// Separate approved-R6-basis route. This does not raise old PC/default caps.
inline constexpr const char* trustedR6Suite="R7-MCL-BN254-HYRAX-A3-TRUSTED-R6-BASIS-v1";
inline constexpr std::size_t maxTrustedR6Dot=4096;
struct DotWitness {std::vector<Fr>values;Fr blind=0;Hidden evaluation;};
struct Measurement {std::uint64_t guard_payload_bytes=0,core_payload_bytes=0,public_payload_bytes=0;double guard_seconds=0,core_seconds=0;};
Measurement& measurement();
std::vector<G1> generators(std::size_t);
G1 vectorCommit(const std::vector<Fr>&,const Fr&);
// Standard separate-input Schnorr knowledge guard, then PRIVATE literal Fig7–8.
// Guard is linear communication, not part of the paper's logarithmic core.
// No offline receipt/registered-input bypass exists in this phase.
void dotProduct(IO&,const G1&xi,const Handle&,const std::vector<Fr>&public_a,const DotWitness*,Random*);
// approved_spec is a verifier-owned external-anchor spec, never a P-supplied basis.
// Both sides independently derive r1::generators(approved_spec).g.
void dotProductTrustedR6Basis(IO&,const r1::PCSpec&approved_spec,const G1&xi,const Handle&,const std::vector<Fr>&public_a,const DotWitness*,Random*);
// One selected §6.1 folded row relation. Not a proof of every row's knowledge,
// padding correctness, model origin, or fixed-C_W registration.
void opening(IO&,const PC&,const Query&,const PCSecret*,const Hidden*,Random*);
}
