#include "r5b_trust.hpp"
#include "paper_r1/wire.hpp"
#include <filesystem>
#include <chrono>
#include <fstream>
#include <iostream>
namespace r1=zkgpt::r1;using namespace zkgpt;
int main(){try{r1::init();r2::ModelQueryPlan p;p.r5b_candidate=true;p.shape.layer_count=1;p.circuit_id=std::string(64,'a');auto spec=r1::PCSpec::make("test/W",1,1,r1::Layout::ColumnMajor);p.sources.push_back({spec,{}});G1 zero;zero.clear();r1::Commitment w{spec,{zero}};
 auto fails=[&](auto f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}r1::require(rejected,"untrusted model metadata accepted");};
 fails([&]{r5b::loadAnchor("","","",p);});
 std::ostringstream m;m<<"ZKGPT-R5B-MODEL-MANIFEST-v1\ncheckpoint-revision unit-test-fake-not-a-model\ncheckpoint-sha256 "<<std::string(64,'0')<<"\nexporter-sha256 "<<std::string(64,'0')<<"\nprofile "<<r5b::kProfile<<"\nprofile-sha256 "<<r5b::profileHash()<<"\ntable-sha256 062c686f919b356c9f8c47acada933bc2683c13ac8886a0cb881239e807f21dc\nlayout r5b-oneblock-padded-output-major-W-v1\nparameter-count 120\n";
 for(int i=0;i<48;++i)m<<"fc_"<<i<<".bin "<<std::string(64,'0')<<"\nfc_"<<i<<"_bias.bin "<<std::string(64,'0')<<'\n';for(int i=0;i<24;++i)m<<"ln_"<<i<<".bin "<<std::string(64,'0')<<'\n';auto text=m.str();r1::Bytes mb(text.begin(),text.end());r5b::parseManifest(mb);
 const auto dir=std::filesystem::path("r5b-trust-unit-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(dir);auto manifest=(dir/"manifest.txt").string(),path=(dir/"anchor.txt").string();r1::writeFile(manifest,mb);auto mh=r5b::digestHex(mb);r5b::Anchor a{r5b::kProfile,r5b::profileHash(),mh,p.circuit_id,"r5b-op-model/"+mh,w};auto b=r5b::encodeAnchor(a);r1::writeFile(path,b);auto sha=r5b::digestHex(b);auto accepted=r5b::loadAnchor(path,sha,manifest,p);r5b::requireAnchorW(accepted,w);
 fails([&]{r5b::loadAnchor(path,std::string(64,'0'),manifest,p);});
 auto badw=w;badw.rows[0]=r1::generators(spec).u;fails([&]{r5b::requireAnchorW(accepted,badw);});
 auto bad=a;bad.profile="r4-partial-candidate-v1";auto changed=r5b::encodeAnchor(bad);r1::writeFile((dir/"wrong-profile").string(),changed);fails([&]{r5b::loadAnchor((dir/"wrong-profile").string(),r5b::digestHex(changed),manifest,p);});
 bad=a;bad.model_id="PROVER-CHOSEN-MODEL";changed=r5b::encodeAnchor(bad);r1::writeFile((dir/"wrong-model").string(),changed);fails([&]{r5b::loadAnchor((dir/"wrong-model").string(),r5b::digestHex(changed),manifest,p);});
 auto altered=mb;altered.back()='X';r1::writeFile((dir/"wrong-manifest").string(),altered);fails([&]{r5b::loadAnchor(path,sha,(dir/"wrong-manifest").string(),p);});
 auto corrupt=b;corrupt.push_back(0);fails([&]{r5b::decodeAnchor(corrupt);});
 std::cout<<"PASS external anchor: normal, missing pin, wrong pin, W replacement, model/profile/manifest mismatch, noncanonical encoding; no publisher certification\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
