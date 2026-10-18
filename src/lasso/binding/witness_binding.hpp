#pragma once
#include "lasso_types.hpp"
#include <memory>
namespace route_b::binding {
using namespace route_b::lasso;
struct Region {
  std::string name;
  std::uint32_t offset = 0, count = 0;
};
struct Position {
  std::uint32_t region = 0, index = 0;
};
struct QueryLayout {
  std::string id, kind;
  bool allow_alias = false;
  std::vector<Position> positions;
};
struct WitnessLayoutDescriptor {
  std::uint32_t version = 1, field = 1, curve = 1, backend = 1, padding = 1,
                witness_length = 0;
  std::string model_profile, circuit_id;
  std::vector<Region> regions;
  std::vector<QueryLayout> queries;
};
// Provisioned independently by the verifier. No trust anchor comes from the
// artifact.
struct WitnessBindingInstance {
  WitnessLayoutDescriptor layout;
  std::uint32_t selected_query = 0;
  LassoLookupInstance lookup;
  std::vector<G1> c0;
};
struct WitnessBindingProof {
  Digest statement_digest{};
  LassoProof lookup;
  Opening query;
  std::vector<Opening> rows;
};
class VerifiedWitnessBindingClaim {
  VerifiedWitnessBindingClaim() = default;
  friend std::unique_ptr<const VerifiedWitnessBindingClaim>
  verifyBinding(const WitnessBindingInstance &, const WitnessBindingProof &,
                std::string *);

public:
  VerifiedWitnessBindingClaim(const VerifiedWitnessBindingClaim &) = delete;
  VerifiedWitnessBindingClaim &
  operator=(const VerifiedWitnessBindingClaim &) = delete;
  Digest c0_digest{}, query_commitment_digest{}, layout_identifier{},
      instance_digest{};
  std::uint32_t query_length = 0, protocol_version = 1;
  std::vector<Fr> row_point;
  Fr evaluation = 0;
  std::uint64_t transcript_challenges = 0;
};
// Caller must rebuild/approve the registry independently. Only raw unsigned
// range values and registered Exp t/E positions; no chunk/selector semantics.
WitnessLayoutDescriptor
layoutFromTrustedRegistry(const WitnessRegistry &, std::uint32_t witness_length,
                          const std::string &model_profile,
                          const std::string &circuit_id);
void validateLayout(const WitnessLayoutDescriptor &);
std::vector<std::size_t> mapping(const WitnessBindingInstance &);
Bytes encodeBindingInstance(const WitnessBindingInstance &);
WitnessBindingInstance decodeBindingInstance(const Bytes &);
Bytes encodeBindingProof(const WitnessBindingProof &);
WitnessBindingProof decodeBindingProof(const Bytes &);
Digest layoutDigest(const WitnessLayoutDescriptor &);
Digest c0Digest(const WitnessBindingInstance &);
Digest bindingInstanceDigest(const WitnessBindingInstance &);
std::vector<G1> commitWitness(const std::vector<Fr> &);
WitnessBindingProof proveBinding(const WitnessBindingInstance &,
                                 const std::vector<Fr> &witness,
                                 const std::vector<Fr> &query);
// Unchecked relation equality for regenerated hostile-prover tests; still
// generates real IPAs.
WitnessBindingProof proveBindingUnchecked(const WitnessBindingInstance &,
                                          const std::vector<Fr> &,
                                          const std::vector<Fr> &);
std::unique_ptr<const VerifiedWitnessBindingClaim>
verifyBinding(const WitnessBindingInstance &, const WitnessBindingProof &,
              std::string *error = nullptr);
} // namespace route_b::binding
