#pragma once
#include "paper_r7_streaming_master.hpp"
#include <ostream>
namespace zkgpt::r7::oneblock {
struct Obligation {std::string id,family,source;std::uint64_t domain=0,rounds=0;};
struct Coverage {
 R7Context context;std::vector<Obligation>required;std::vector<WitnessObject>objects;
 std::vector<std::string>model_regions;std::string native_json;std::size_t fc_layers=0,native_terminals=0,source_objects=0;
 std::uint64_t object_padded_coordinates=0,full_representation_response_bytes=0;
};
Coverage discover(unsigned blocks=1);
Coverage discover(const R6PublicArtifacts&);
std::string identity(const Coverage&);
void writeCoverage(std::ostream&,const Coverage&);
}
