#pragma once
#include "framework.hpp"
#include "paper_r2/r6_model.hpp"
#include <functional>
namespace zkgpt::r7 {
using R6PublicArtifacts=r6::PublicCircuit;
std::unique_ptr<R6PublicArtifacts> buildR6PublicArtifacts(unsigned blocks=1);
R7Context buildR6Context(const std::string& independently_pinned_manifest_sha256,const R6PublicArtifacts&);
R7Context buildR6Context(const std::string& independently_pinned_manifest_sha256,unsigned blocks=1);
std::vector<r1::InputSpan> nativeXSpans(const R7Context&);
std::vector<r1::InputSpan> nativeXSpans(const R7Context&,const R6PublicArtifacts&);
std::string checkManifest(const std::string&file,const std::string&pin,unsigned blocks=1);
// Frozen R6 exceptions may contain scalar values; only this sanitized boundary
// may surround private computation before a CLI/logging boundary.
void executePrivateWitness(const std::function<void()>&);
}
