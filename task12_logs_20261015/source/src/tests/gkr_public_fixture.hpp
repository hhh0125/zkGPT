#pragma once
#include "circuit.h"
namespace gkr_test {
inline constexpr unsigned kSmallFconnCount=4,kSmallFconnDimension=4;
// Public wiring only. No prover, witness, model parameter or private file access.
void buildSmallFconnPublicCircuit(layeredCircuit&);
}
