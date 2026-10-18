#pragma once
#include "circuit.h"
#include "model_binding.hpp"
#include <string>
#include <vector>

namespace route_b::production {
enum class Mode { BridgeV1=1, LassoV2=2 };
// Verifier deployment policy, never deserialized from a proof or manifest.
// full12() explicitly approves the existing publisher anchor for P0 metadata.
// It makes no claim about provenance from an official model checkpoint.
struct TrustedDeployment {
  Mode mode=Mode::BridgeV1;
  std::string policy_id, model_id, implementation_profile, anchor_sha256;
  ModelAnchor anchor;
  static TrustedDeployment full12(Mode);
};
struct Span {
  std::size_t offset=0,count=0;
  unsigned bias_bits=0; // 0 => no bias, otherwise add 2^(bias_bits-1).
};
struct PublicModelLayout {
  std::size_t n0=0,N0=0,ell=0,output_offset=0;
  RangeLayoutPlan ranges;
  std::vector<RegisteredExpQuery> exp;
  std::vector<std::size_t> constants;
  std::vector<neuralNetwork::ParameterRegion> parameters;
  // Ordered, lossless contiguous-run encoding of every actual ori_id entry.
  // Indexed [layer*2+port]; no digest substitutes for the mapping.
  std::vector<std::vector<Span>> ports;
};
std::string hexDigest(const BindingDigest&);
void require(bool,const std::string&);
std::size_t paddedSize(std::size_t);
std::size_t dimensions(std::size_t);
PublicModelLayout rebuildProductionLayout(const TrustedDeployment&,const layeredCircuit&,const WitnessRegistry&);
}
