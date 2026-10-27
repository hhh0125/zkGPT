#pragma once
#include "oneblock_sumcheck.hpp"
#include "master_registration.hpp"
#include "framework.hpp"
#include <utility>
namespace zkgpt::r7::oneblock {
using PublicEntries=std::vector<std::pair<std::size_t,Fr>>;
struct TrustedPublicEntries {PublicEntries x,y,constants;};
// The context and statement are verifier-owned inputs. No coordinate mapping
// or expected value is imported from a prover-generated source plan.
TrustedPublicEntries trustedPublicEntries(const R7Statement&,const R7Context&);
// Binds the complete public index/value list before fresh verifier position
// coins, then proves its weighted equality against the SAME Registered Master.
// Duplicate source indices contribute additively to the public kernel.
void publicBinding(IO&,const masterB::Registered&,const master3::Secret*onlyP,
                   Value master,const PublicEntries&,Random*onlyP_random,
                   const std::string&role);
}
