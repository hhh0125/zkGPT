#pragma once
#include "paper_r1/core.hpp"
#include "paper_r2/model_queries.hpp"
#include <array>
#include <optional>
#include <memory>
class LLM;
class prover;
namespace zkgpt::r7 {
namespace crypto { class IO; }
using r1::Fr; using r1::Bytes; using r1::Digest;
enum class Code { Ok, Reject, Incomplete, Unimplemented };
enum class Component : std::size_t { Commitment, HiddenScalar, Opening, CommittedSumcheck, GKR, Memory, MappedInput, Bridge, Count };
std::array<Component,8> allComponents();
const char* name(Component);
const char* name(Code);
struct Result {
    Code code=Code::Incomplete;
    Component component=Component::Commitment;
    std::string detail;
};
struct R7Statement {
    std::uint32_t version=1, blocks=1;
    std::string profile_sha256, circuit_id, layout_id, layout_sha256, model_manifest_sha256;
    std::string output_semantics="r6-final-hidden-state-signed64-padded-32x1024";
    std::string commitment_suite="r7-hiding-pc-rowmajor-v2";
    std::uint64_t master_actual=0, master_padded=0;
    std::vector<Fr> x,y;
};
enum class ObjectKind { BaseMaster, MappedChild, ModelW, RangeDigit, MemoryRead, MemoryFinal, ExpHelper };
struct WitnessObject {
    std::string id,parent;
    ObjectKind kind=ObjectKind::BaseMaster;
    std::uint64_t actual=0,padded=0;
    // Coordinate maps are verifier-owned R6 source plans, never a new flat Master.
    std::vector<r1::InputSpan> spans;
    bool materialized=false;
};
struct R7Context {
    r2::ModelQueryPlan plan;
    std::string model_manifest_sha256, layout_sha256;
    std::vector<WitnessObject> objects;
};
struct R7Proof {
    Digest statement_digest{};
    std::array<std::optional<Bytes>,8> parts;
};
// Stores actual R6 val[0] once. Padded tail is an implicit zero domain;
// auxiliary witnesses have separate identities and are not appended to it.
class R7WitnessStore {
    std::unique_ptr<LLM> network_;
    std::unique_ptr<::prover> execution_;
    std::vector<WitnessObject> objects_;
  public:
    R7WitnessStore();
    ~R7WitnessStore();
    R7WitnessStore(const R7WitnessStore&)=delete;
    R7WitnessStore& operator=(const R7WitnessStore&)=delete;
    void generateR6(const R7Context&, const std::string& manifest);
    const std::vector<Fr>& baseMaster() const;
    std::vector<Fr> publicX() const;
    std::vector<Fr> publicY(const R7Context&) const;
    bool legacyCommitmentAbsent() const;
    ::prover& executionForR7();
    struct FCContraction { std::vector<Fr> activation,weight;Fr output=0;std::size_t layer=0; };
    FCContraction firstFCContraction(const R7Context&) const;
    const std::vector<WitnessObject>& objects() const { return objects_; }
};
class R7Transcript {
    r1::Fs transcript_;
  public:
    explicit R7Transcript(const R7Statement&);
    explicit R7Transcript(const std::string& interactive_domain);
    void record(const std::string&,const Bytes&);
    void append(Component,const Bytes&);
    Digest digest() const { return transcript_.digest(); }
    Fr challenge(Component,const std::string&);
};
// These typed requests retain object and parent identities for Task0-B/C/D/E.
// No clear hidden evaluation or blind is serialized by the current envelope.
struct CommitmentRequest { std::string object_id,parameter_id; const std::vector<Fr>& values; };
struct OpeningRequest { std::string object_id; Bytes commitment; std::vector<Fr> point; Bytes hidden_evaluation; };
struct R7Commitment { virtual ~R7Commitment()=default; virtual Result commit(const CommitmentRequest&,R7Transcript&)=0; virtual Result verifyCommitments(const R7Statement&,const R7Proof&,R7Transcript&) const=0; };
struct R7HiddenScalar { virtual ~R7HiddenScalar()=default; virtual Result verifyHiddenScalar(const R7Statement&,const R7Proof&,R7Transcript&) const=0; };
struct R7Opening { virtual ~R7Opening()=default; virtual Result open(const OpeningRequest&,R7Transcript&)=0; virtual Result verifyOpenings(const R7Statement&,const R7Proof&,R7Transcript&) const=0; };
struct R7CommittedSumcheck { virtual ~R7CommittedSumcheck()=default; virtual Result verifySumcheck(const R7Statement&,const R7Proof&,R7Transcript&) const=0; };
struct R7GKR { virtual ~R7GKR()=default; virtual Result verifyGKR(const R7Context&,const R7Statement&,const R7Proof&,R7Transcript&) const=0; };
struct R7Memory { virtual ~R7Memory()=default; virtual Result verifyMemory(const R7Context&,const R7Statement&,const R7Proof&,R7Transcript&) const=0; };
struct R7MappedInput { virtual ~R7MappedInput()=default; virtual Result verifyMappedInput(const R7Context&,const R7Statement&,const R7Proof&,R7Transcript&) const=0; virtual Result verifyBridge(const R7Context&,const R7Statement&,const R7Proof&,R7Transcript&) const=0; };
R7Statement makeStatement(const R7Context&,std::vector<Fr> x,std::vector<Fr> y);
Result validateStatement(const R7Statement&,const R7Context&);
class R7OneblockVerifier {
  public:
    Result verify(const R7Statement&,const R7Proof&,const R7Context&) const;
    Result verifyLocalInteractive(crypto::IO&,const std::string&mode,std::size_t n) const;
};
struct ProverOutput { Result result; R7Statement statement; R7Proof proof; };
class R7OneblockProver {
  public:
    ProverOutput run(const R7Context&,const std::string& manifest,R7WitnessStore&) const;
    Result runLocalInteractive(crypto::IO&,const std::string&mode,std::size_t n,const std::string&attack) const;
};
}
