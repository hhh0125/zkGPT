#include "model_queries.hpp"
#include "models.hpp"
#include "paper_r1/wire.hpp"
#include <iostream>
namespace r1=zkgpt::r1;
using namespace zkgpt::r2;
int main(int argc,char **argv){try{
 r1::init();r1::require(argc==3,"usage: r2_model_source_verify EXTERNALLY_SELECTED_STATEMENT SOURCE_PROOF");
 auto s=r1::parseLookupSpec(r1::readFile(argv[1]));
 LLM nn(1,12,64,768,3072);auto c=nn.buildPublicCircuit();
 auto plan=planModelQueries(c,nn.getWitnessRegistry(),nn.parameterRegions());
 r1::require(s.input.public_values.size()>=65536,"missing public X/Y");
 std::vector<Fr>x,y;for(std::size_t i=0;i<32768;++i){x.push_back(s.input.public_values[i].value);y.push_back(s.input.public_values[32768+i].value);}
 r1::require(!s.input.children.empty(),"missing W source");
 requireModelQueryStatement(plan,s,s.input.children.back().commitment,x,y,"local-quantized-gpt2-source-test-not-publisher-approved");
 auto p=r1::parseMappedInput(s,r1::readFile(argv[2]));r1::Fs t("real-model-source-only-v1");r1::verifyMappedInput(s.input,p,t);
 std::cout<<"ACCEPT SOURCE BINDING ONLY: real one-block X/Y/W, 52 Range regions, 6336 Exp, constants and padding. No integrated model or publisher approval.\n";return 0;
}catch(const std::exception&e){std::cerr<<"REJECT "<<e.what()<<'\n';return 2;}}
