#pragma once

#include "sigma.hpp"
#include "circuit.h"

class prover;
namespace zkgpt::r7::oneblock {
using crypto::Fr;

struct GKRTerminal {
 std::uint32_t layer=0;
 unsigned port=0,matrix_point=0;
 std::size_t actual=0,padded=0;
 std::vector<Fr> point;
 crypto::Hidden value;
};

// This result proves only the arithmetic reduction to these exact terminal
// handles. Every terminal must subsequently be authenticated against the SAME
// registered Master in the SAME live session. It is never an ACCEPT receipt.
struct PendingGKR {
 std::vector<GKRTerminal> terminals;
 std::size_t layers=0,matrices=0,carries=0,rounds=0;
 const bool master_authenticated=false;
};

// V supplies its independently reconstructed public graph and no prover or RNG.
// P supplies the existing finite witness without invoking legacy PC/Open.
// Native R6 GKR output is the public zero; X/Y are separately Master-bound.
PendingGKR runHiddenGKR(crypto::IO&,const layeredCircuit&,::prover* onlyP,
                       std::size_t sequence_length,crypto::Random* onlyP_random);

}
