#pragma once
#include "models.hpp"
#include "model_queries.hpp"
#include "model_resources.hpp"
#include <ostream>
namespace zkgpt::r6 {
inline void configure(LLM &n){n.setR6EngineeringCandidate();}
void auditGraph(const layeredCircuit&,const LLM&,const r2::ModelQueryPlan&);
// Owns one fully audited public build. No mutable aliases or copies of the
// network/graph escape, so serialization can rely on constructor validation.
class PublicCircuit final {
 LLM network_;
 layeredCircuit graph_;
 r2::ModelQueryPlan plan_;
 public:
 explicit PublicCircuit(unsigned blocks);
 PublicCircuit(const PublicCircuit&)=delete;
 PublicCircuit& operator=(const PublicCircuit&)=delete;
 PublicCircuit(PublicCircuit&&)=delete;
 PublicCircuit& operator=(PublicCircuit&&)=delete;
 const LLM& network()const{return network_;}
 const layeredCircuit& graph()const{return graph_;}
 const r2::ModelQueryPlan& plan()const{return plan_;}
};
void writePlan(std::ostream&,const layeredCircuit&,const LLM&,const r2::ModelQueryPlan&);
void writePlan(std::ostream&,const PublicCircuit&);
}
