from pathlib import Path
p=Path('src/witness_registry.hpp');s=p.read_text().replace('#include <algorithm>','#include "exp_lookup.hpp"\n#include <algorithm>') # cycle! use struct declaration local instead
s=s.replace('#include "exp_lookup.hpp"\n','')
s=s.replace('class WitnessRegistry {','struct RegisteredExpQuery { std::size_t operator_id, query_id, t_witness_index, E_witness_index; };\n\nclass WitnessRegistry {')
s=s.replace('public:\n    void setShape','public:\n    void addExpQueries(std::size_t op,std::size_t to,std::size_t eo,std::size_t count) {\n        if (!exp_queries_.empty() && exp_queries_.back().operator_id>=op)\n            throw std::logic_error("duplicate or unordered exp operator");\n        for(std::size_t i=0;i<count;i++) exp_queries_.push_back({op,i,to+i,eo+i});\n    }\n    const std::vector<RegisteredExpQuery> &expQueries() const {return exp_queries_;}\n    void setShape')
s=s.replace('constraints_.clear();','constraints_.clear();\n        exp_queries_.clear();')
s=s.replace('std::vector<RangeConstraint> constraints_;','std::vector<RangeConstraint> constraints_;\n    std::vector<RegisteredExpQuery> exp_queries_;')
p.write_text(s)
p=Path('src/neuralNetwork.cpp');s=p.read_bytes().decode();needle='    const size_t witness_instance = softmax_witness_instance++;';s=s.replace(needle,needle+'\n    witness_registry.addExpQueries(witness_instance,\n        orgsize+10+2*head_len+2*triangular,\n        orgsize+10+2*head_len+3*triangular,triangular);');p.write_bytes(s.encode())
p=Path('src/model_binding.hpp');s=p.read_text().replace('#include "neuralNetwork.hpp"','#include "neuralNetwork.hpp"\n#include "exp_lookup.hpp"');s=s.replace('struct BoundArtifact {','struct BoundArtifact {\n  std::uint32_t strict_protocol_version=3;\n  ExpLookupProof exp_lookup;')
s=s.replace('// Checks model/input equality and the complete legacy Range/GKR relation.','BoundArtifact bindStrictArtifactV4(const ModelAnchor &, const ZkGPTPublicStatement &,\n    const ZkGPTProof &, const std::vector<Fr> &witness, const std::vector<Fr> &input, int threads);\n// Explicit legacy acceptance has no exponential membership property.\nbool verifyLegacyBoundArtifactV3(const ModelAnchor &, const BoundArtifact &,\n    const std::vector<Fr> &, std::string *error=nullptr);\n// Strict v4 checks model/input, exponential lookup and complete Range/GKR.\n// Checks model/input equality and the complete legacy Range/GKR relation.')
p.write_text(s)
p=Path('src/model_binding.cpp');s=p.read_text();s=s.replace('  return bindingHash("zkGPT.quantization.profile.v3", c.b);','  c.dig(canonicalExpTableId());\n  c.u(kExpTableSize);\n  c.u(kExpTailStart);\n  return bindingHash("zkGPT.quantization.profile.v4", c.b);')
needle='bool verifyBoundArtifact(const ModelAnchor &a, const BoundArtifact &b,';i=s.index(needle);j=s.index('\nstd::vector<uint8_t> encodeModelAnchor',i)
s=s[:i]+'''namespace {
Fr expContext(const ModelAnchor &a,const BoundArtifact &b) {
  auto t=transcript(a,b); t.appendU64("strict-protocol-version",4);
  const auto id=canonicalExpTableId();t.appendBytes("trusted-exp-table",id.data(),id.size());
  return t.challenge("exp-lookup-context.v4");
}
}
BoundArtifact bindStrictArtifactV4(const ModelAnchor &a,const ZkGPTPublicStatement &s,
    const ZkGPTProof &p,const std::vector<Fr> &w,const std::vector<Fr> &x,int threads) {
  auto b=bindModelArtifact(a,s,p,w,x);b.strict_protocol_version=4;
  b.exp_lookup=proveExpLookup(s.range_statement,w,expContext(a,b),threads);
  return b;
}
bool verifyLegacyBoundArtifactV3(const ModelAnchor &a,const BoundArtifact &b,
    const std::vector<Fr> &x,std::string *error) {
  if(b.strict_protocol_version!=3){if(error)*error="legacy v3 version required";return false;}
  return verifyModelInputBinding(a,b,x,error) && verifyZkGPT(b.statement,b.proof,error);
}
bool verifyBoundArtifact(const ModelAnchor &a,const BoundArtifact &b,
    const std::vector<Fr> &x,std::string *error) {
  if(b.strict_protocol_version!=4){if(error)*error="strict v4 required: legacy proof has no exp lookup";return false;}
  // Identity and table are checked before expensive original verification.
  if(!verifyModelInputBinding(a,b,x,error))return false;
  if(!verifyExpLookup(b.statement.range_statement,b.exp_lookup,expContext(a,b),error))return false;
  return verifyZkGPT(b.statement,b.proof,error);
}
''' + s[j:]
s=s.replace('  c.str("ZKGPT-STRICT-ARTIFACT-V3");','  require(a.strict_protocol_version==3 || a.strict_protocol_version==4,"strict encode version");\n  c.str(a.strict_protocol_version==4?"ZKGPT-STRICT-ARTIFACT-V4":"ZKGPT-STRICT-ARTIFACT-V3");')
s=s.replace('  return c.b;\n}\nBoundArtifact decodeBoundArtifact', '''  if(a.strict_protocol_version==4) {
    c.dig(a.exp_lookup.exp_table_id);c.u(a.exp_lookup.query_count);
    c.blob(serializeRangeProof(a.exp_lookup.payload));
  }
  return c.b;
}
BoundArtifact decodeBoundArtifact''')
s=s.replace('  require(c.str() == "ZKGPT-STRICT-ARTIFACT-V3", "strict artifact version");\n  BoundArtifact a;', '''  auto magic=c.str();
  require(magic=="ZKGPT-STRICT-ARTIFACT-V3" || magic=="ZKGPT-STRICT-ARTIFACT-V4","strict artifact version");
  BoundArtifact a;a.strict_protocol_version=magic=="ZKGPT-STRICT-ARTIFACT-V4"?4:3;''')
s=s.replace('  c.end();\n  return a;\n}\nstd::vector<Fr> readPublicEmbedding', '''  if(a.strict_protocol_version==4) {
    a.exp_lookup.exp_table_id=c.dig();a.exp_lookup.query_count=c.u();
    require(deserializeRangeProof(c.blob(),a.exp_lookup.payload,&e),"exp lookup decode");
  }
  c.end();
  return a;
}
std::vector<Fr> readPublicEmbedding''')
p.write_text(s)
p=Path('src/main_demo_llm.cpp');s=p.read_text().replace('auto bound=bindModelArtifact(anchor,statement,proof,p.val[0],x);','auto bound=bindStrictArtifactV4(anchor,statement,proof,p.val[0],x,range_thread_count);')
# locate thread var below
s=s.replace('"strict v3 requires','"strict v4 requires').replace('.strict-v3.bin','.strict-v4.bin');s=s.replace('        strict_bytes=encodeBoundArtifact(bound);','        if(!verifyExpLookup(statement.range_statement,bound.exp_lookup,expContextUnavailable,&error)) {}\n        strict_bytes=encodeBoundArtifact(bound);') if False else s
p.write_text(s)
p=Path('src/main_model_binding.cpp');s=p.read_text().replace('"v3 model/input binding and legacy Range/GKR "\n                   "ACCEPT\\nQuantization exp-table membership: UNPROVED\\n"','"Strict v4 model/input + Range/GKR + exp lookup ACCEPT\\n"');p.write_text(s)
