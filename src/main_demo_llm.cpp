
#undef NDEBUG
#include "circuit.h"
#include "neuralNetwork.hpp"
#include "verifier.hpp"
#include "models.hpp"
#include "global_var.hpp"
#include <iostream>

#include "range_prover.hpp"
#include "hyrax_rp.hpp"
#include "stats.hpp"
using namespace mcl::bn;
using namespace std;




int main(int argc, char **argv) 
{
    initPairing(mcl::BN254);
    
    // range prover
    printf("Start range proving...\n");
    range_prover range_prover(12, 12, 64, 768, 3072, 30, 32, 1); // 12 layer, 12 head, 64 channel, 768 hidden dim, 3072 MLP dim, 30 seq len, 32 threads
    range_prover.init();
    range_prover.build();
    // 证明者运行，证明输入值的范围
    double range_prover_time = range_prover.prove();


    // gkr
    prover p;
    // GPT-2 small: 12 blocks, 12 heads, hidden size 768 and MLP size 3072.
    LLM nn(12, 12, 64, 768, 3072);
    // Keep the unsqueezed circuit until residual layers are included in the
    // squeeze scheduler. This preserves the exact GPT-2 data dependencies.
    nn.create(p, 0);
    verifier v(&p, p.C);
    // v.range_prove(range_prover_time);
    v.prove(32); // prove with 32 threads
    stats::print_stats();

}

