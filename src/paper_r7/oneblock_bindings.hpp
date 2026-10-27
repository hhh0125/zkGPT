#pragma once
#include "oneblock_sumcheck.hpp"
#include "master_registration.hpp"

namespace zkgpt::r7::oneblock {

// The verifier constructs index and coordinate_bias from its trusted native
// source plan. They are never imported from a prover-supplied mapping receipt.
// For j<actual the mapped value is Master[index(j)]+bias(j); padded tail is 0.
struct MappedMasterClaim {
 std::string id;
 std::size_t actual=0,padded=0;
 std::function<std::size_t(std::size_t)> index;
 Hidden value;
 std::vector<Fr> point;
 Fr bias=0;
 // When present, overrides the uniform bias above coordinate by coordinate.
 std::function<Fr(std::size_t)> coordinate_bias={};
};

// Authenticates exactly this fixed set of handles against the supplied live
// Registered Master. Success is scoped to these claims; it does not change a
// PendingGKR, fill a coverage bitmap, or authorize whole-Oneblock acceptance.
void authenticateMappedClaims(IO&,const masterB::Registered&,
 const master3::Secret* onlyP_secret,const Value& onlyP_master,
 const std::vector<MappedMasterClaim>&,Random* onlyP_random,const std::string&role);

}
