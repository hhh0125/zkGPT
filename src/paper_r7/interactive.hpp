#pragma once
#include "crypto.hpp"
#include "wire.hpp"
namespace zkgpt::r7::crypto {
class PipeIO final:public IO{
 bool prover_;std::string sid_,attack_;std::uint64_t seq_=0,total_=0,sent_=0;bool attacked_=false;OSRandom coins_;R7Transcript transcript_{"zkGPT.R7.independent-interactive.v1"};Bytes public_transcript_;
 void send(const std::string&,const Bytes&);Bytes receive(const std::string&);
 public:
 PipeIO(bool prover,const std::string&mode,const std::string&attack="");
 bool prover()const override{return prover_;}std::string session()const override{return sid_;}
 Bytes message(const std::string&,const Bytes& = {})override;Fr coin(const std::string&)override;
 Digest prefix()const override{return transcript_.digest();}
 std::uint64_t proofBytes()const{return sent_;}
 const std::string& attack()const{return attack_;}
 const Bytes& publicTranscript()const{return public_transcript_;}
};
}
