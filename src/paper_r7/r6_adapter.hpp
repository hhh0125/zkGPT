#pragma once
#include "framework.hpp"
#include <functional>
namespace zkgpt::r7 {
R7Context buildR6Context(const std::string& independently_pinned_manifest_sha256,unsigned blocks=1);
std::string checkManifest(const std::string&file,const std::string&pin,unsigned blocks=1);
// Frozen R6 exceptions may contain scalar values; only this sanitized boundary
// may surround private computation before a CLI/logging boundary.
void executePrivateWitness(const std::function<void()>&);
}
