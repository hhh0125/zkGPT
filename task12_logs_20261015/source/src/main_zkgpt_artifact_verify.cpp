#include "zkgpt_serialization.hpp"
#include "zkgpt_artifact_io.hpp"

#include "global_var.hpp"
#include "timer.hpp"

#include <iostream>
#include <stdexcept>

int main(int argc,char **argv) {
    if (argc!=3) {
        std::cerr << "usage: zkgpt_artifact_verify STATEMENT PROOF\n";
        return 2;
    }
    try {
        initPairing(mcl::BN254);
        timer decode_timer;
        decode_timer.start();
        const auto statement_bytes=readZkGPTArtifactFile(
            argv[1],kMaxZkGPTStatementFileBytes,"statement");
        const auto proof_bytes=readZkGPTArtifactFile(
            argv[2],kMaxZkGPTProofFileBytes,"proof");
        ZkGPTPublicStatement statement;
        ZkGPTProof proof;
        std::string error;
        if (!deserializeZkGPTPublicStatement(
                statement_bytes,statement,&error))
            throw std::runtime_error("statement decode failed: "+error);
        validateZkGPTDeploymentBudget(statement);
        if (!deserializeZkGPTProof(proof_bytes,proof,&error))
            throw std::runtime_error("proof decode failed: "+error);
        decode_timer.stop();
        std::cout << "Artifact read+decode time: "
                  << decode_timer.elapse_sec() << "s\n";
        if (!verifyZkGPT(statement,proof,&error))
            throw std::runtime_error(error);
        std::cout << "zkGPT artifact verified: statement_bytes="
                  << statement_bytes.size() << ", proof_bytes="
                  << proof_bytes.size() << '\n';
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "zkGPT artifact rejected: " << exception.what() << '\n';
        return 1;
    }
}
