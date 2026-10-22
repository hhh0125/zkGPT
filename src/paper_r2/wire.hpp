#pragma once
#include "protocol.hpp"
#include "paper_r1/wire.hpp"
namespace zkgpt::r2 {
r1::Bytes serializeStatement(const GKRPublicStatement &g,const r1::LookupBatchSpec &s);
std::pair<GKRPublicStatement,r1::LookupBatchSpec> parseStatement(const r1::Bytes &b);
r1::Bytes serializeProof(const GKRPublicStatement &g,const r1::LookupBatchSpec &s,const Proof &p);
Proof parseProof(const GKRPublicStatement &g,const r1::LookupBatchSpec &s,const r1::Bytes &b);
}
