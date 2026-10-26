#pragma once
#include "models.hpp"
#include "model_queries.hpp"
#include "model_resources.hpp"
#include <ostream>
namespace zkgpt::r6 {
inline void configure(LLM &n){n.setR6EngineeringCandidate();}
void auditGraph(const layeredCircuit&,const LLM&,const r2::ModelQueryPlan&);
void writePlan(std::ostream&,const layeredCircuit&,const LLM&,const r2::ModelQueryPlan&);
}
