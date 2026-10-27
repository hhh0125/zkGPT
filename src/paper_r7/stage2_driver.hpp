#pragma once
#include "stage2_fixture.hpp"
#include "stage2_sumcheck.hpp"
#include "interactive.hpp"
namespace zkgpt::r7::stage2 {
struct ExperimentStats{std::vector<Phase>phases;std::string registry_digest;double reduction_seconds=0;};
ExperimentStats experiment(PipeIO&,const TrustedRegistry*,PrivateFixture*,const std::string&mode,std::size_t k,const std::string&attack);
}
