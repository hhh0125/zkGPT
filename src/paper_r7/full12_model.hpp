#pragma once
#include "oneblock_model.hpp"
#include "classified_layout.hpp"
#include "full12_model_pin.hpp"
namespace zkgpt::r7::full12 {
inline constexpr const char*anchorPath="reports/R7-FULL12-UNRESTRICTED-20261029-v1/full12-chunks.anchor";
inline constexpr const char*bundleSuite="R7-FULL12-ROWMAJOR-CHUNK-ANCHOR-v1";
struct Trust {std::string pin,partition;std::vector<oneblock::model::Trust>chunks;};
Bytes encodeTrust(const R7Context&,const std::vector<r5b::Anchor>&);
Trust loadTrust(const R7Context&,const std::string&path=anchorPath);
std::string trustIdentity(const Trust&);
std::vector<oneblock::model::Secret>prepare(const Trust&,const R7Context&,const oneblock::Value&,const std::string&,crypto::Random&);
}
