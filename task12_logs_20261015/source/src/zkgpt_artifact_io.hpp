#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Deployment ingress budgets. These are deliberately smaller than the 1 GiB
// wire-format ceiling: the frozen v2 full-scale artifacts are about 0.54 MiB
// and 15.91 MiB, respectively. Keeping the policy here ensures the verifier
// checks the file size before allocating a buffer controlled by an attacker.
inline constexpr std::size_t kMaxZkGPTStatementFileBytes = 16ULL << 20;
inline constexpr std::size_t kMaxZkGPTProofFileBytes = 64ULL << 20;

std::vector<std::uint8_t> readZkGPTArtifactFile(
    const std::string &path, std::size_t maximum_bytes,
    const char *artifact_kind);
