// Post-validation public mapping audit; no witness generation or cryptographic acceptance.
#include "classified_commitments.hpp"
#include "oneblock_plan.hpp"
#include "oneblock_public.hpp"
#include "opt3a_reference.hpp"
#include "paper_r2/r6_model.hpp"
#include "wire.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
using namespace zkgpt::r7;using namespace oneblock;
namespace {
Bytes read(std::istream&in,std::size_t n){need(n<=4u*1024*1024,"census frame bound");Bytes b(n);in.read(reinterpret_cast<char*>(b.data()),n);need(bool(in),"census truncated proof");return b;}
std::uint64_t number(std::istream&in,unsigned n){auto b=read(in,n);Reader r{b};return r.number(n);}
std::string quoted(const std::string&s){std::string out="\"";for(auto c:s){need(static_cast<unsigned char>(c)>=32,"census nonprintable ID");if(c=='\\'||c=='\"')out+='\\';out+=c;}return out+'\"';}
}
int main(int argc,char**argv){try{
 need(argc==5,"usage: compact_claim_census oneblock|full12 PROOF STATEMENT VERIFIED_ROOT_ID");r1::init();
 unsigned blocks=std::string(argv[1])=="full12"?12:1;auto discovery=discover(blocks);auto layout=classified::makeLayout(discovery.context);
 std::ifstream statement(argv[3],std::ios::binary);Bytes sb{std::istreambuf_iterator<char>(statement),{}};auto s=R7Wire::decodeStatement(sb);need(validateStatement(s,discovery.context).code==Code::Ok,"census statement context");
 auto plan=classified::singlePlan(layout,s.x);auto layout_id=classified::identity(layout);single::Public pc;pc.plan=plan;
 layeredCircuit graph;{master3::QuietStdout quiet;LLM nn(blocks,12,64,768,3072);zkgpt::r6::configure(nn);graph=nn.buildPublicCircuit();}
 auto entries=trustedPublicEntries(s,discovery.context);auto pub=entries.y;pub.insert(pub.end(),entries.constants.begin(),entries.constants.end());
 std::vector<std::unordered_map<std::uint32_t,std::size_t>>public_cells(layout.roots.size());std::vector<std::uint32_t>public_max(layout.roots.size());
 for(const auto&c:plan.public_cells){public_cells[c.source-1].emplace(c.index,c.x_index);public_max[c.source-1]=std::max(public_max[c.source-1],std::uint32_t(c.index));}
 std::ifstream proof(argv[2],std::ios::binary);need(read(proof,8)==Bytes({'Z','K','R','7','F','S','1',0}),"census proof magic");read(proof,number(proof,4));
 bool header=false,sealed=false,complete=false;std::set<std::string>ids;std::vector<std::pair<std::string,Digest>>claims;std::uint64_t sequence=1,total_coordinates=0;
 // Transcript initialization and derived coin events are not P frames in the
 // file. The complete independent verifier already checked their exact order.
 while(!complete){auto tag=number(proof,1);auto event=number(proof,8);need(tag=='P' && event>sequence,"census proof event order");sequence=event;auto n=number(proof,8);need(n && n<=256,"census label bound");auto lb=read(proof,n);std::string label(lb.begin(),lb.end());auto payload=read(proof,number(proof,8));
  if(label=="SingleHyrax/header"){need(payload==single::descriptor(plan),"census descriptor mismatch");header=true;}
  if(label.rfind("SingleHyrax/rows/",0)==0){need(header,"census rows before header");Reader r{payload};auto count=r.number();need(count<=256 && pc.rows.size()+count<=plan.rows,"census row dimensions");for(std::size_t i=0;i<count;++i)pc.rows.push_back(r.point());r.end();}
  if(label=="SingleHyrax/sealed"){Reader r{payload};need(r.text()==single::suite && r.text()==argv[4],"census sealed identity");r.text();r.end();need(pc.rows.size()==plan.rows && single::identity(pc)==argv[4],"census verified root mismatch");sealed=true;}
  if(label.rfind("Classified/defer/",0)==0){need(sealed && label=="Classified/defer/"+std::to_string(claims.size()),"census unsealed/out-of-order declaration");Reader r{payload};need(r.text()==classified::suite && r.text()==layout_id,"census classified identity");auto id=r.text(),map_id=r.text();auto bytes=r.raw(32);Digest expected{};std::copy(bytes.begin(),bytes.end(),expected.begin());r.text();r.text();r.text();r.point();auto padded=r.number(8);need(padded && padded<=r1::kMaxVector && ids.insert(id).second,"census claim bound/duplicate");for(unsigned j=0;j<r1::log2exact(padded);++j)r.scalar();r.end();
   std::function<classified::Located(std::size_t)>map;
   auto view=layout.views.find(map_id);
   if(view!=layout.views.end()){const auto*v=&view->second;need(v->padded==padded,"census view dimensions");map=[&layout,v](std::size_t j){return classified::locate(layout,*v,j);};}
   else if(id=="public/Y-constants" && map_id=="native-base/"+id){need(classified::power2(pub.size())==padded,"census public shape");map=[&](std::size_t j)->classified::Located{if(j>=pub.size())return {{},Fr(0),true};return {classified::baseCoordinate(layout,pub[j].first),Fr(0),false};};}
   else {need(map_id=="native-base/"+id,"census unknown map family");std::vector<std::string>parts;std::istringstream split(id);std::string part;while(std::getline(split,part,'/'))parts.push_back(part);need(parts.size()==6 && parts[0]=="gkr" && parts[1]=="layer" && parts[3]=="terminal" && (parts[4]=="u"||parts[4]=="v") && (parts[5]=="0"||parts[5]=="1"),"census unknown native terminal");auto layer=std::stoul(parts[2]);need(layer && layer<graph.circuit.size(),"census terminal layer");auto&l=graph.circuit.at(layer);bool port=parts[4]=="v";auto width=port?l.bit_length_v[0]:l.bit_length_u[0];need(width>=0 && (std::size_t(1)<<width)==padded,"census terminal dimension");auto*indices=port?&l.ori_id_v:&l.ori_id_u;map=[&layout,indices](std::size_t j)->classified::Located{if(j>=indices->size())return {{},Fr(0),true};return {classified::baseCoordinate(layout,indices->at(j)),Fr(0),false};};}
   classified::opt3a_reference::MapHash digest;std::vector<std::uint64_t>private_counts(layout.roots.size()),fixed_counts(layout.roots.size());std::uint64_t constants=0,public_x=0;
   for(std::size_t j=0;j<padded;++j){auto at=map(j);if(!at.constant){auto k=at.coordinate.root;need(k<layout.roots.size() && at.coordinate.index<layout.roots[k].actual,"census coordinate bounds");if(!public_cells[k].empty() && at.coordinate.index<=public_max[k]){auto found=public_cells[k].find(at.coordinate.index);if(found!=public_cells[k].end()){++fixed_counts[k];++public_x;at.bias+=s.x.at(found->second);at.constant=true;at.coordinate={};}}if(!at.constant)++private_counts[k];}if(at.constant)++constants;digest.append(at);}
   need(digest.finish()==expected,"census reconstructed mapping digest differs from verified proof");claims.push_back({id,expected});total_coordinates+=padded;
   std::cout<<"{\"kind\":\"claim\",\"id\":"<<quoted(id)<<",\"map_id\":"<<quoted(map_id)<<",\"map_sha256\":\""<<hex(expected)<<"\",\"padded\":"<<padded<<",\"constant_coordinates\":"<<constants<<",\"public_X_authority_coordinates\":"<<public_x<<",\"sources\":[";bool first=true;
   for(std::size_t k=0;k<private_counts.size();++k)if(private_counts[k]||fixed_counts[k]){if(!first)std::cout<<',';first=false;std::cout<<"{\"source_index\":"<<k+1<<",\"private_coordinates\":"<<private_counts[k]<<",\"fixed_public_coordinates\":"<<fixed_counts[k]<<'}';}std::cout<<"]}\n"<<std::flush;
  }
  if(label=="UnifiedInput/all-claims-fixed"){Reader r{payload};need(r.text()==unified::suite && r.number()==claims.size(),"census claim summary count");for(const auto&c:claims){need(r.text()==c.first && r.raw(32)==Bytes(c.second.begin(),c.second.end()),"census claim summary mismatch");}r.end();complete=true;}
 }
 need(complete && !claims.empty(),"census incomplete");std::cout<<"{\"kind\":\"summary\",\"scope\":\"post-validation-public-map-census\",\"claims\":"<<claims.size()<<",\"coordinates_checked\":"<<total_coordinates<<",\"source_segments\":"<<plan.segments.size()<<",\"all_mapping_digests_match_verified_proof\":true,\"verified_root_id\":\""<<argv[4]<<"\"}\n";return 0;
 }catch(const std::exception&e){std::cerr<<"CLAIM_CENSUS_FAILED "<<e.what()<<'\n';return 1;}}
