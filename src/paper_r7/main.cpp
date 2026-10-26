#include "r6_adapter.hpp"
#include "wire.hpp"
#include "paper_r1/wire.hpp"
#include "paper_r2/r5b_trust.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace zkgpt::r7;
namespace {
Bytes bounded(const std::string&file,std::size_t cap){std::ifstream f(file,std::ios::binary|std::ios::ate);zkgpt::r1::require(bool(f),"cannot read input");auto n=f.tellg();zkgpt::r1::require(n>=0 && std::uint64_t(n)<=cap,"input file size bound");f.seekg(0);Bytes out(static_cast<std::size_t>(n));zkgpt::r1::require(bool(f.read(reinterpret_cast<char*>(out.data()),out.size())),"input read failed");return out;}
}
int main(int argc,char**argv){try{
 zkgpt::r1::init();
#ifdef R7_VERIFIER_MAIN
 zkgpt::r1::require(argc==6,"usage: r7_oneblock_verify STATEMENT ENVELOPE MANIFEST PINNED_MANIFEST_SHA256 PINNED_STATEMENT_SHA256");
 auto bytes=bounded(argv[1],R7Wire::maxStatementBytes);
 zkgpt::r1::require(zkgpt::r5b::digestHex(bytes)==argv[5],"independent public statement SHA256 mismatch");
 checkManifest(argv[3],argv[4]);auto context=buildR6Context(argv[4]);
 auto result=R7OneblockVerifier{}.verify(R7Wire::decodeStatement(bytes),R7Wire::decodeProof(bounded(argv[2],8*R7Wire::maxPartBytes+1024)),context);
#else
 zkgpt::r1::require(argc==4,"usage: r7_oneblock_prove NEW_OUTPUT_DIRECTORY MANIFEST PINNED_MANIFEST_SHA256");
 checkManifest(argv[2],argv[3]);auto context=buildR6Context(argv[3]);
 auto root=std::filesystem::canonical(std::filesystem::current_path());auto dir=std::filesystem::weakly_canonical(std::filesystem::absolute(argv[1]));auto rel=dir.lexically_relative(root);
 zkgpt::r1::require(!rel.empty() && *rel.begin()!=".." && !std::filesystem::exists(dir),"output must be new and inside workspace");
 R7WitnessStore store;auto out=R7OneblockProver{}.run(context,argv[2],store);
 if(out.result.code==Code::Reject){std::cout<<"REJECT "<<out.result.detail<<'\n';return 2;}
 std::filesystem::create_directories(dir);
 auto statement=R7Wire::encodeStatement(out.statement);
 zkgpt::r1::writeFile((dir/"oneblock.statement").string(),statement);
 zkgpt::r1::writeFile((dir/"oneblock.incomplete-envelope").string(),R7Wire::encodeProof(out.proof));
 std::ofstream report(dir/"framework.json");
 report<<"{\"status\":\"FRAMEWORK_CONNECTED\",\"production_gate\":\"BLOCKED_CRYPTOGRAPHIC_DESIGN\",\"may_enter_stage2\":false,\"integer_witness_validated\":true,\"legacy_pc_called\":false,\"cryptographic_proof_generated\":false,\"base_master_actual\":"<<context.plan.master.actual<<",\"base_master_padded\":"<<context.plan.master.padded<<",\"auxiliary_and_source_objects\":"<<context.objects.size()-1<<",\"statement_sha256\":\""<<zkgpt::r5b::digestHex(statement)<<"\",\"profile_sha256\":\""<<out.statement.profile_sha256<<"\",\"model_manifest_sha256\":\""<<out.statement.model_manifest_sha256<<"\",\"layout_sha256\":\""<<out.statement.layout_sha256<<"\"}\n";
 zkgpt::r1::require(bool(report),"framework result write failed");
 std::cout<<"FRAMEWORK_CONNECTED real R6 oneblock integer computation and witness validated; independent verification remains incomplete\n";
 auto result=R7OneblockVerifier{}.verify(out.statement,out.proof,context);
#endif
 std::cout<<name(result.code)<<" component="<<name(result.component)<<" "<<result.detail<<"\nproduction_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN may_enter_stage2=false\n";
 return result.code==Code::Reject?2:3;
}catch(const std::exception&e){std::cerr<<"REJECT "<<e.what()<<'\n';return 2;}}
