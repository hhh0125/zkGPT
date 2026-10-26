#pragma once
#include "gkr_verifier.hpp"
class prover;
#include "paper_r1/lookup_batch.hpp"
namespace zkgpt::r2 {
// Verifier receives its approved public circuit and LookupBatchSpec separately.
// It must never replace those with a layout supplied inside a proof.
struct Proof {
    GKRProof gkr;
    r1::OpenProof master_binding;
    MleOpeningProof gkr_input_binding;
    r1::LookupBatchProof lookup;
};
Proof proveR2(::prover &prover,GKRPublicStatement &gkr_statement,
    const r1::LookupBatchSpec &lookup,
    const std::vector<Fr> &master,const std::vector<std::vector<Fr>> &children,
    const r1::LookupProgress &progress={});
std::vector<GKRInputTerminalClaim> verifyR2(const layeredCircuit &circuit,
    const GKRPublicStatement &gkr_statement,const r1::LookupBatchSpec &lookup,const Proof &p,const r1::LookupProgress &progress={});
} // namespace zkgpt::r2
