#pragma once
#include "neuralNetwork.hpp"
#include "exp_lookup.hpp"
#include "zkgpt_serialization.hpp"
using BindingDigest = std::array<std::uint8_t, 32>;
struct ModelAnchor {
  std::string checkpoint_identifier = "unverified-local-quantized-gpt2";
  BindingDigest model_id{}, profile_id{}, parameter_digest{},
      circuit_fingerprint{};
  RangePublicStatement
      layout; // only shape, val0 size/domain and masked row commitments
  std::vector<neuralNetwork::ParameterRegion> parameters;
};
struct BoundArtifact {
  std::uint32_t strict_protocol_version=3;
  ExpLookupProof exp_lookup;
  BindingDigest model_id{}, profile_id{}, input_id{};
  ZkGPTPublicStatement statement;
  ZkGPTProof proof;
  std::vector<HyraxIpaProof> equality_openings;
};
ModelAnchor prepareModelAnchor(const neuralNetwork &, const prover &,
                               const BindingDigest &profile);
BoundArtifact bindModelArtifact(const ModelAnchor &,
                                const ZkGPTPublicStatement &,
                                const ZkGPTProof &,
                                const std::vector<Fr> &witness,
                                const std::vector<Fr> &input);
BoundArtifact bindStrictArtifactV4(const ModelAnchor &, const ZkGPTPublicStatement &,
    const ZkGPTProof &, const std::vector<Fr> &witness, const std::vector<Fr> &input, int threads);
// Explicit legacy acceptance has no exponential membership property.
bool verifyLegacyBoundArtifactV3(const ModelAnchor &, const BoundArtifact &,
    const std::vector<Fr> &, std::string *error=nullptr);
// Strict v4 checks model/input, exponential lookup and complete Range/GKR.
// Checks model/input equality and the complete legacy Range/GKR relation.
// This does NOT prove Softmax exponential-table membership: the legacy
// circuit has no such constraint. A matching profile_id is not runtime
// quantization attestation; see docs/model_identity_input_binding.md.
bool verifyBoundArtifact(const ModelAnchor &, const BoundArtifact &,
                         const std::vector<Fr> &input,
                         std::string *error = nullptr);
// Identity relation only; used during proof construction and isolated tests.
// A true result is never a complete inference-proof acceptance.
bool verifyModelInputBinding(const ModelAnchor &, const BoundArtifact &,
                             const std::vector<Fr> &input,
                             std::string *error = nullptr);
std::vector<std::uint8_t> encodeModelAnchor(const ModelAnchor &);
ModelAnchor decodeModelAnchor(const std::vector<std::uint8_t> &);
std::vector<std::uint8_t> encodeBoundArtifact(const BoundArtifact &);
BoundArtifact decodeBoundArtifact(const std::vector<std::uint8_t> &);
BindingDigest bindingHash(const std::string &domain,
                          const std::vector<std::uint8_t> &);
BindingDigest quantizationImplementationProfile();
std::vector<Fr> readPublicEmbedding(const std::string &);
