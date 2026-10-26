#include "r5b_trust.hpp"
#include "models.hpp"
#include "paper_r1/wire.hpp"
#include <filesystem>
#include <iostream>
namespace r1=zkgpt::r1;using namespace zkgpt;
int main(int argc,char**argv){try{r1::init();r1::require(argc==4,"usage: r5b_model_preprocess PARAMETER_DIR MANIFEST NEW_ANCHOR");r1::require(!std::filesystem::exists(argv[3]),"operator anchor already exists");auto mb=r1::readFile(argv[2]);auto manifest=r5b::parseManifest(mb);LLM nn(1,12,64,768,3072);nn.setR5BEngineeringCandidate();auto c=nn.buildPublicCircuit();auto plan=r2::planModelQueries(c,nn.getWitnessRegistry(),nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);auto w=r5b::preprocessWeights(plan,nn.parameterRegions(),manifest,argv[1]);auto cw=r1::commit(plan.sources.back().spec,w);auto mh=r5b::digestHex(mb);r5b::Anchor anchor{r5b::kProfile,r5b::profileHash(),mh,plan.circuit_id,"r5b-op-model/"+mh,cw};auto bytes=r5b::encodeAnchor(anchor);r1::writeFile(argv[3],bytes);std::cout<<"OPERATOR-TRUSTED PREPROCESSING ONLY; deterministic W PC derived from all pinned parameter bytes and exact padded layout. Publisher certification and profile approval unclaimed.\nANCHOR_SHA256 "<<r5b::digestHex(bytes)<<"\nMANIFEST_SHA256 "<<mh<<"\nCIRCUIT "<<plan.circuit_id<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"REJECT preprocessing: "<<e.what()<<'\n';return 2;}}
