#include "gkr_serialization.hpp"
#include "range_serialization.hpp"
#include "zkgpt_artifact_io.hpp"
#include "zkgpt_serialization.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <unistd.h>
#include <vector>

namespace {

void require(bool condition,const std::string &message) {
    if (!condition) throw std::runtime_error(message);
}

void putU32(std::vector<std::uint8_t> &bytes,std::size_t offset,
            std::uint32_t value) {
    require(offset+4<=bytes.size(),"test mutation offset is out of range");
    for (unsigned i=0;i<4;++i)
        bytes[offset+i]=static_cast<std::uint8_t>(value>>(8*i));
}

std::size_t findBytes(const std::vector<std::uint8_t> &haystack,
                      const std::string &needle) {
    const auto it=std::search(haystack.begin(),haystack.end(),
        needle.begin(),needle.end());
    require(it!=haystack.end(),"test marker was not found");
    return static_cast<std::size_t>(it-haystack.begin());
}

void requireRejected(bool accepted,const std::string &error,
                     const std::string &name) {
    require(!accepted,name+" was accepted");
    require(!error.empty(),name+" did not return a diagnostic");
}

void setProcessBudgets() {
    const rlimit address_space{512ULL<<20,512ULL<<20};
    if (setrlimit(RLIMIT_AS,&address_space)!=0)
        throw std::runtime_error("cannot set decoder-test address-space limit");
    const rlimit cpu_time{20,20};
    if (setrlimit(RLIMIT_CPU,&cpu_time)!=0)
        throw std::runtime_error("cannot set decoder-test CPU limit");
}

RangePublicStatement smallRangeStatement() {
    RangePublicStatement statement;
    statement.val0_log_size=0;
    statement.val0_generator_domain={"zkGPT/main",1};
    statement.range_generator_domain={"zkGPT/range",kRangeGeneratorCountV2};
    statement.shape={32,1,1,1,1,1};
    return statement;
}

GKRProof smallGkrProof() {
    GKRProof proof;
    proof.output_evaluation=Fr(0);
    proof.lasso.input_evaluation=Fr(0);
    proof.lasso.mapping_evaluation=Fr(0);
    proof.lasso.input_opening.ipa.final_witness=Fr(0);
    proof.transcript_binding=Fr(0);
    return proof;
}

RangeProof smallRangeProof() {
    RangeProof proof;
    proof.transcript_binding=Fr(0);
    return proof;
}

void testTopLevelLengthsAndTails() {
    ZkGPTPublicStatement statement;
    statement.model_shape={32,1,1,1,1,1};
    statement.output_claim.mle_evaluation=Fr(0);
    statement.range_statement=smallRangeStatement();
    ZkGPTProof proof;
    proof.gkr_proof=smallGkrProof();
    proof.range_proof=smallRangeProof();
    proof.transcript_binding=Fr(0);

    const auto statement_bytes=serializeZkGPTPublicStatement(statement);
    const auto proof_bytes=serializeZkGPTProof(proof);
    std::string error;
    ZkGPTProof decoded_proof;
    ZkGPTPublicStatement decoded_statement;

    auto huge_nested=proof_bytes;
    putU32(huge_nested,8,0xffffffffU);
    requireRejected(deserializeZkGPTProof(
        huge_nested,decoded_proof,&error),error,"extreme nested length");

    auto truncated=statement_bytes;
    truncated.pop_back();
    error.clear();
    requireRejected(deserializeZkGPTPublicStatement(
        truncated,decoded_statement,&error),error,"truncated statement");

    auto trailing=proof_bytes;
    trailing.push_back(0);
    error.clear();
    requireRejected(deserializeZkGPTProof(
        trailing,decoded_proof,&error),error,"trailing proof byte");
}

void testGkrCountsAndCanonicalField() {
    const auto baseline=serializeGKRProof(smallGkrProof());
    std::uint8_t fr_encoding[64]{};
    Fr zero=0;
    const std::size_t fr_size=zero.serialize(fr_encoding,sizeof(fr_encoding));
    require(fr_size>0,"invalid Fr serialized size");
    const std::size_t layer_count_offset=8+fr_size;
    GKRProof decoded;
    std::string error;

    auto too_many_layers=baseline;
    putU32(too_many_layers,layer_count_offset,4097);
    requireRejected(deserializeGKRProof(
        too_many_layers,decoded,&error),error,"extreme GKR layer count");

    auto within_limit_but_dense=baseline;
    putU32(within_limit_but_dense,layer_count_offset,4096);
    within_limit_but_dense.insert(
        within_limit_but_dense.begin()+layer_count_offset+4,4096,0);
    error.clear();
    requireRejected(deserializeGKRProof(within_limit_but_dense,decoded,&error),
                    error,"dense in-range GKR layer vector");

    auto noncanonical_fr=baseline;
    std::fill(noncanonical_fr.begin()+8,
              noncanonical_fr.begin()+8+fr_size,0xff);
    error.clear();
    requireRejected(deserializeGKRProof(
        noncanonical_fr,decoded,&error),error,"non-canonical Fr");
}

void testRangeNestedCountsAndCanonicalPoint() {
    const auto statement_bytes=serializeRangePublicStatement(
        smallRangeStatement());
    RangePublicStatement decoded_statement;
    std::string error;

    auto extreme_generator=statement_bytes;
    const std::string main_domain="zkGPT/main";
    const std::size_t main_pos=findBytes(extreme_generator,main_domain);
    putU32(extreme_generator,main_pos+main_domain.size(),
           kMaxGeneratorCount+1);
    requireRejected(deserializeRangePublicStatement(
        extreme_generator,decoded_statement,&error),error,
        "extreme generator count");

    // ZKRS + version + val0_log_size; the next field is the G1 vector count.
    auto noncanonical_g1=statement_bytes;
    putU32(noncanonical_g1,12,1);
    noncanonical_g1.insert(noncanonical_g1.begin()+16,
                           G1::getSerializedByteSize(),0xff);
    error.clear();
    requireRejected(deserializeRangePublicStatement(
        noncanonical_g1,decoded_statement,&error),error,
        "non-canonical G1");

    auto nested=serializeRangeProof(smallRangeProof());
    putU32(nested,8,4096);
    nested.insert(nested.begin()+12,4096*4,0);
    RangeProof decoded_proof;
    error.clear();
    require(deserializeRangeProof(nested,decoded_proof,&error),
            "bounded empty nested-vector encoding was not decoded: "+error);
    require(decoded_proof.chunk_commitments.size()==4096,
            "nested vector count changed during decoding");
    require(serializeRangeProof(decoded_proof)==nested,
            "nested vector canonical round-trip changed bytes");
}

void testFileIngressLimit() {
    char path[]="/tmp/zkgpt-decoder-budget-XXXXXX";
    const int fd=mkstemp(path);
    if (fd<0) throw std::runtime_error("cannot create decoder-test file");
    struct Cleanup {
        int fd;
        const char *path;
        ~Cleanup() { if (fd>=0) close(fd); unlink(path); }
    } cleanup{fd,path};
    require(ftruncate(fd,static_cast<off_t>(
        kMaxZkGPTStatementFileBytes+1))==0,"cannot size decoder-test file");
    bool rejected=false;
    try {
        (void)readZkGPTArtifactFile(path,kMaxZkGPTStatementFileBytes,
                                   "statement");
    } catch (const std::length_error &) {
        rejected=true;
    }
    require(rejected,"oversized input file was not rejected before allocation");
}

void testCircuitBuildBudget() {
    ZkGPTPublicStatement statement;
    statement.model_shape={32,13,12,64,768,3072};
    bool rejected=false;
    try {
        validateZkGPTDeploymentBudget(statement);
    } catch (const std::length_error &) {
        rejected=true;
    }
    require(rejected,"extreme circuit layer count passed deployment budget");
}

} // namespace

int main() {
    try {
        setProcessBudgets();
        initPairing(mcl::BN254);
        const auto start=std::chrono::steady_clock::now();
        testTopLevelLengthsAndTails();
        testGkrCountsAndCanonicalField();
        testRangeNestedCountsAndCanonicalPoint();
        testFileIngressLimit();
        testCircuitBuildBudget();
        const double elapsed=std::chrono::duration<double>(
            std::chrono::steady_clock::now()-start).count();
        require(elapsed<10.0,"decoder resource tests exceeded wall budget");
        std::printf("decoder resource tests passed: wall=%.3fs, "
                    "RLIMIT_AS=512MiB, RLIMIT_CPU=20s\n",elapsed);
        return 0;
    } catch (const std::exception &exception) {
        std::fprintf(stderr,"decoder resource tests failed: %s\n",
                     exception.what());
        return 1;
    }
}
