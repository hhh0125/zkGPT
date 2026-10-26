#include "thread_joiner.hpp"
#include "r6_system.hpp"
#include "r6_model.hpp"
#include "r6_trust.hpp"
#include "paper_r1/wire.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
using namespace zkgpt;
int main(int argc,char**argv){try{r1::init();r1::require(argc==7 && std::string(argv[1])=="--blocks","usage: r6_model_preprocess --blocks 1|12 PARAMETER_DIR MANIFEST NEW_ANCHOR NEW_REPORT");auto blocks=r2::parseModelBlocks(argv[2]);
 r1::require(!std::filesystem::exists(argv[5]) && !std::filesystem::exists(argv[6]),"R6 preprocessing refuses existing anchor/report");
 r1::require(zkgpt::workerThreadCount()<=8,"R6 preprocessing requires ZKGPT_THREADS<=8");r1::require(r6::effectiveRoom().memory>=(std::size_t(56)<<30) && r6::effectiveRoom().pids>=64,"R6 preprocessing requires24GiB budget plus32GiB available reserve");
 auto start=std::chrono::steady_clock::now();LLM nn(blocks,12,64,768,3072);r6::configure(nn);auto c=nn.buildPublicCircuit();auto p=r2::planModelQueries(c,nn.getWitnessRegistry(),nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);r6::auditGraph(c,nn,p);
 auto mb=r1::readFile(argv[4]);auto m=r6::parseManifest(mb,blocks);auto w=r6::preprocessWeights(p,nn.parameterRegions(),m,argv[3]);
 auto checked=r6::compareProductionMapping(p,nn.parameterRegions(),w,argv[3]);r6::checkParameterFiles(m,argv[3]);std::cerr<<"R6 production mapping matched "<<checked<<" scalars; committing existing deterministic PC\n";
 auto cm=r1::commit(p.sources.back().spec,w);r6::checkParameterFiles(m,argv[3]);r5b::Anchor a{r5b::kProfile,r5b::profileHash(),r5b::digestHex(mb),p.circuit_id,"r6-op-model/"+std::to_string(blocks)+"/"+r5b::digestHex(mb),cm};
 auto bytes=r6::encodeAnchor(a);r1::writeFile(argv[5],bytes);auto pin=r5b::digestHex(bytes);auto loaded=r6::loadAnchor(argv[5],pin,argv[4],p);r5b::requireAnchorW(loaded,cm);
 std::ofstream out(argv[6]);out<<"{\"blocks\":"<<blocks<<",\"profile_sha256\":\""<<a.profile_hash<<"\",\"circuit\":\""<<p.circuit_id<<"\",\"anchor\":\""<<argv[5]<<"\",\"anchor_sha256\":\""<<pin<<"\",\"manifest\":\""<<argv[4]<<"\",\"manifest_sha256\":\""<<a.manifest_hash<<"\",\"model_id\":\""<<a.model_id<<"\",\"W_actual\":"<<cm.spec.actual<<",\"W_padded\":"<<cm.spec.padded<<",\"PC_rows\":"<<cm.spec.rows<<",\"PC_columns\":"<<cm.spec.columns<<",\"all_parameter_files_SHA_checked\":120,\"production_reader_matched_scalars\":"<<checked<<",\"padded_tail_zero_scalars\":"<<w.size()-checked<<",\"operator_trusted\":true,\"publisher_certified\":false,\"full12_numeric_witness_built\":false,\"full12_proof_run\":false,\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"}\n";out.close();r1::require(!out.fail(),"R6 preprocessing report write failed");std::cout<<"PASS R6 full parameter mapping and deterministic external anchor; ANCHOR_SHA256 "<<pin<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
