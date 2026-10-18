#include "tests/gkr_test_fixture.hpp"
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include "tests/gkr_public_fixture.hpp"
#include "utils.hpp"
namespace {
void check(bool value,const std::string& text) {if(!value)throw std::runtime_error(text);}
}
int main(int argc,char** argv) {
    try {
        mcl::bn::initPairing(mcl::BN254);
        gkr_test::SmallFconnFixture fixture;
        ChallengeTraceRecorder prover_trace,verifier_trace;
        GKRProofBuilder builder(&fixture.proverInstance(),fixture.circuit(),
            fixture.statement().model_shape,fixture.statement().val0_generator_domain,
            &prover_trace,"task12/gkr/session");
        builder.build();
        auto statement=builder.zkPublicStatement();auto proof=builder.zkProof();std::string error;
        check(verifyGKRZk(fixture.circuit(),statement,proof,&error,&verifier_trace),"real GKR ZK completeness: "+error);
        check(proof.output_evaluation==Fr(9),"independent fixture output evaluation is not nine");
        check(compareChallengeTraces(prover_trace.entries,verifier_trace.entries,&error),"GKR ZK trace mismatch: "+error);
        check(prover_trace.field_labels==verifier_trace.field_labels,"GKR field-message label trace mismatch");
        bool saw_masked_round=false;
        for(const auto& label:prover_trace.field_labels) {
            check(label.rfind("gkr.phase1.",0)!=0 && label.rfind("gkr.phase2.",0)!=0 &&
                label.rfind("gkr.layer.final_",0)!=0 && label!="lasso.input_evaluation" &&
                label!="lasso.sumcheck.a"&&label!="lasso.sumcheck.b"&&label!="lasso.sumcheck.c" &&
                label.find(".final_f")==std::string::npos&&label.find(".final_g")==std::string::npos,
                "unmasked legacy witness field appeared in ZK transcript: "+label);
            if(label.find(".round.value")!=std::string::npos)saw_masked_round=true;
        }
        check(saw_masked_round,"masked round trace missing");
        for(const auto dimension:{std::uint64_t{1}<<32,(std::uint64_t{1}<<32)+4,(std::uint64_t{1}<<30)+1}) {
            auto oversized=statement;
            oversized.arithmetic.model_shape.hidden_dimension=dimension;
            oversized.arithmetic.model_shape.head_dimension=dimension;
            ChallengeTraceRecorder oversized_trace;
            check(!verifyGKRZk(fixture.circuit(),oversized,proof,&error,&oversized_trace),"oversized model dimension accepted");
            check(oversized_trace.entries.empty()&&error.find("before narrowing")!=std::string::npos,
                "oversized dimension was not rejected before narrowing/challenges");
        }
        bool guarded=false;try{builder.proof();}catch(const std::exception&){guarded=true;}check(guarded,"legacy getter exposed raw transcript in ZK mode");
        auto bytes=serializeGKRZkProof(fixture.circuit(),proof);GKRZkProof decoded;
        check(deserializeGKRZkProof(bytes,fixture.circuit(),decoded,&error),"ZK proof decode: "+error);
        check(verifyGKRZk(fixture.circuit(),statement,decoded,&error),"ZK proof roundtrip: "+error);
        const auto statement_bytes=serializeGKRZkStatement(statement);GKRZkPublicStatement decoded_statement;
        check(deserializeGKRZkStatement(statement_bytes,decoded_statement,&error),"ZK statement codec: "+error);
        check(verifyGKRZk(fixture.circuit(),decoded_statement,decoded,&error),"ZK statement roundtrip: "+error);
        std::filesystem::path output_directory;
        std::vector<std::string> manifest_rows;
        if(argc==2) {
            output_directory=argv[1];
            check(std::filesystem::create_directory(output_directory),"fixture output directory already exists");
        } else check(argc==1,"usage: gkr_zk_tests [new-public-fixture-directory]");
        const auto exportRaw=[&](const std::string& name,const zkgpt::zk::Bytes& sb,
            const zkgpt::zk::Bytes& pb,int expected,const std::string& context) {
            if(output_directory.empty())return;
            const auto stem="case-"+std::to_string(manifest_rows.size());
            for(const auto& item:{std::make_pair(stem+".statement.bin",sb),std::make_pair(stem+".proof.bin",pb)}) {
                std::ofstream out(output_directory/item.first,std::ios::binary);
                check(bool(out),"cannot create public fixture");
                out.write(reinterpret_cast<const char*>(item.second.data()),item.second.size());check(bool(out),"fixture write failed");
            }
            std::ostringstream row;row<<"{\"name\":"<<std::quoted(name)<<",\"stem\":"<<std::quoted(stem)
                <<",\"expected_exit\":"<<expected<<",\"expected_context\":"<<std::quoted(context)<<"}";
            manifest_rows.push_back(row.str());
        };
        exportRaw("positive",statement_bytes,bytes,0,statement.context);
        exportRaw("wrong-expected-context",statement_bytes,bytes,2,"wrong/external/context");
        size_t accepted=1,rejected=0;
        auto reject=[&](const std::string& name,const GKRZkPublicStatement& s,const GKRZkProof& p) {
            check(!verifyGKRZk(fixture.circuit(),s,p,&error),name+" accepted");++rejected;
            std::cout<<"CRYPTO_REJECT "<<name<<": "<<error<<'\n';
            exportRaw(name,serializeGKRZkStatement(s),serializeGKRZkProof(fixture.circuit(),p),1,s.context);
        };
        size_t normal=0,matrix=0;
        for(size_t i=0;i<proof.layers.size();++i) {
            if(proof.layers[i].layer_type==static_cast<unsigned>(layerType::FCONN))matrix=i;else normal=i;
        }
        auto p=proof;p.layers[normal].normal_mask.coefficients[0]+=builder.zkParameters().U;reject("wrong mask",statement,p);
        p=proof;p.layers[normal].normal_mask.rounds[0][0]+=Fr(1);reject("wrong normal round",statement,p);
        p=proof;p.layers[normal].terminals[0]+=builder.zkParameters().U;reject("wrong GKR terminal",statement,p);
        p=proof;p.layers[matrix].matrix_v.product.st+=Fr(1);reject("wrong FCONN second parent",statement,p);
        p=proof;p.lasso.input_commitment+=builder.zkParameters().U;reject("wrong Lasso hidden claim",statement,p);
        p=proof;p.lasso.input_opening.z[0]+=Fr(1);reject("wrong hidden Hyrax response",statement,p);
        p=proof;p.transcript_binding+=Fr(1);reject("wrong final binding",statement,p);
        auto s=statement;s.arithmetic.val0_commitment[0]+=builder.zkParameters().U;reject("wrong original val0 row",s,proof);
        s=statement;s.context="wrong/session";reject("replayed context",s,proof);
        s=statement;s.arithmetic.output_evaluation+=Fr(1);reject("wrong public output",s,proof);
        s=statement;s.arithmetic.circuit_fingerprint[0]^=1;reject("wrong trusted public circuit",s,proof);
        p=proof;p.layers[0].terminals[1]+=builder.zkParameters().U;reject("wrong shared previous-layer claim",statement,p);
        bytes.push_back(0);check(!deserializeGKRZkProof(bytes,fixture.circuit(),decoded,&error),"trailing codec accepted");
        exportRaw("decode-trailing",statement_bytes,bytes,2,statement.context);
        if(!output_directory.empty()) {
            std::ofstream manifest(output_directory/"manifest.json");manifest<<"[";
            for(size_t i=0;i<manifest_rows.size();++i){if(i)manifest<<",";manifest<<manifest_rows[i];}
            manifest<<"]\n";check(bool(manifest),"manifest write failed");
        }
        GKRProofBuilder fresh(&fixture.proverInstance(),fixture.circuit(),fixture.statement().model_shape,
            fixture.statement().val0_generator_domain,nullptr,"task12/gkr/session");
        fresh.build();
        const auto fresh_bytes=serializeGKRZkProof(fixture.circuit(),fresh.zkProof());
        check(fresh_bytes!=serializeGKRZkProof(fixture.circuit(),proof),"GKR proof randomness reused");
        check(fresh.zkPublicStatement().arithmetic.val0_commitment!=statement.arithmetic.val0_commitment,
            "GKR val0 row blinders reused");
        check(verifyGKRZk(fixture.circuit(),fresh.zkPublicStatement(),fresh.zkProof(),&error),"fresh GKR proof failed: "+error);
        // Exercise actual one-phase and zero-round GKR branches by appending
        // a direct-input unary layer to the SAME four-FCONN production fixture.
        auto& instance=fixture.proverInstance();
        for(const unsigned count:{4U,1U}) {
            instance.C.circuit.emplace_back();instance.C.size=instance.C.circuit.size();
            auto& top=instance.C.circuit.back();initLayer(top,count,layerType::ADD_BIAS);
            top.size_u[0]=count;top.bit_length_u[0]=ceilPow2BitLength(count);
            top.max_bl_u=top.bit_length_u[0];top.max_bl_v=0;
            for(unsigned i=0;i<count;++i){top.ori_id_u.push_back(i);top.uni_gates.emplace_back(i,i,0,1);}
            instance.val.resize(instance.C.size);instance.val.back().assign(count,Fr(0));instance.val.back()[0]=Fr(3);
            ChallengeTraceRecorder edge_trace;
            GKRProofBuilder edge(&instance,instance.C,fixture.statement().model_shape,
                fixture.statement().val0_generator_domain,&edge_trace,"task12/gkr/one-phase");
            edge.build();
            check(verifyGKRZk(instance.C,edge.zkPublicStatement(),edge.zkProof(),&error),"one-phase GKR failed: "+error);
            check(edge.zkProof().layers.front().normal_mask.rounds.size()==static_cast<size_t>(top.max_bl_u),"one-phase round count");
            Fr expected=3;
            for(int j=0;j<top.bit_length;++j){Fr r;const auto& encoding=edge_trace.entries.at(j).challenge_encoding;
                check(r.deserialize(encoding.data(),encoding.size())==encoding.size(),"output point decoding");expected*=Fr(1)-r;}
            check(edge.zkProof().output_evaluation==expected,"independent unary output polynomial");
            if(count==1)check(edge.zkProof().layers.front().normal_mask.rounds.empty(),"zero-round GKR missing");
        }
        instance.C.circuit.emplace_back();instance.C.size=instance.C.circuit.size();
        {
            auto& scalar=instance.C.circuit.back();initLayer(scalar,1,layerType::Sqr);scalar.need_phase2=true;
            scalar.bin_gates.emplace_back(0,0,0,1,0);scalar.ori_id_u.push_back(0);scalar.ori_id_v.push_back(0);
            scalar.size_u[0]=scalar.size_v[0]=1;scalar.bit_length_u[0]=scalar.bit_length_v[0]=0;
            instance.val.resize(instance.C.size);instance.val.back().assign(1,Fr(9));
            GKRProofBuilder scalar_proof(&instance,instance.C,fixture.statement().model_shape,
                fixture.statement().val0_generator_domain,nullptr,"task12/gkr/zero-two-phase");scalar_proof.build();
            check(scalar_proof.zkProof().layers.front().normal_mask.rounds.empty(),"zero-round two-phase GKR missing");
            check(scalar_proof.zkProof().output_evaluation==Fr(9),"scalar square public output");
            check(verifyGKRZk(instance.C,scalar_proof.zkPublicStatement(),scalar_proof.zkProof(),&error),"zero-round two-phase GKR failed: "+error);
        }
        // Constant-zero matrix sources with empty public input maps produce
        // a genuine zero Lasso mapping coefficient, without overriding FS.
        gkr_test::buildSmallFconnPublicCircuit(instance.C);instance.val.resize(instance.C.size);
        for(auto& values:instance.val)for(auto& value:values)value=0;
        for(unsigned fc=0;fc<gkr_test::kSmallFconnCount;++fc) {
            auto& matrix=instance.C.circuit[1+2*fc];matrix.ori_id_u.clear();matrix.ori_id_v.clear();
            matrix.size_u[0]=matrix.size_v[0]=0;matrix.bit_length_u[0]=matrix.bit_length_v[0]=0;
            for(unsigned i=0;i<16;++i)instance.mat_val[fc][i]=0;
        }
        instance.commitInput(instance.gens,1);
        GKRProofBuilder zero(&instance,instance.C,fixture.statement().model_shape,
            fixture.statement().val0_generator_domain,nullptr,"task12/gkr/zero-map");zero.build();
        check(zero.zkProof().lasso.mapping_evaluation.isZero(),"test did not reach zero Lasso mapping");
        check(verifyGKRZk(instance.C,zero.zkPublicStatement(),zero.zkProof(),&error),"zero-mapping GKR failed: "+error);
        auto wrong_zero=zero.zkProof();wrong_zero.lasso.masked.terminal_relation.z+=Fr(1);
        check(!verifyGKRZk(instance.C,zero.zkPublicStatement(),wrong_zero,&error),"zero-mapping terminal tamper accepted");
        std::cout<<"GKR ZK ACCEPT="<<accepted<<" CRYPTO_REJECT="<<rejected<<" bytes="<<bytes.size()-1
            <<" layers="<<proof.layers.size()<<" challenges="<<prover_trace.entries.size()<<'\n';
        return 0;
    }catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
