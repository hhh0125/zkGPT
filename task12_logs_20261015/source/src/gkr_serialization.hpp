#pragma once

#include "gkr_protocol.hpp"

#include <cstdint>
#include <string>
#include <vector>

std::vector<std::uint8_t> serializeGKRProof(const GKRProof &proof);
bool deserializeGKRProof(const std::vector<std::uint8_t> &bytes,
                         GKRProof &proof, std::string *error = nullptr);

zkgpt::zk::Bytes serializeGKRZkProof(const layeredCircuit&,const GKRZkProof&);
bool deserializeGKRZkProof(const zkgpt::zk::Bytes&,const layeredCircuit&,
    GKRZkProof&,std::string* error=nullptr);
zkgpt::zk::Bytes serializeGKRZkStatement(const GKRZkPublicStatement&);
bool deserializeGKRZkStatement(const zkgpt::zk::Bytes&,GKRZkPublicStatement&,
    std::string* error=nullptr);

