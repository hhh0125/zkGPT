#pragma once
#include <models.hpp>
#include "paper_r2_model_queries.hpp"
#include "paper_r2_model_resources.hpp"
#include <ostream>
namespace zkgpt::r6 {
inline void configure(LLM &n){n.setR6EngineeringCandidate();}
// Owns one public graph and the metadata recorded while building it.
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
