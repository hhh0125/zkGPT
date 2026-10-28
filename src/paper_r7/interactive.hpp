#pragma once
#include "crypto.hpp"
#include "wire.hpp"
namespace zkgpt::r7::crypto {
class PipeIO final:public IO{
 bool prover_;std::string sid_,attack_;std::uint64_t seq_=0,total_=0,sent_=0,max_frame_=0,limit_=16*1024*1024,frame_limit_=4096;int audit_fd_=-1;bool attacked_=false;OSRandom coins_;std::unique_ptr<Random>live_coins_;R7Transcript transcript_{"zkGPT.R7.independent-interactive.v1"};Bytes public_transcript_;
 void send(const std::string&,const Bytes&);Bytes receive(const std::string&);
 void audit(const Bytes&);
 public:
 enum class Profile{Default,MasterChunkedV1,OneblockV1,ClassifiedV2};
 PipeIO(bool prover,const std::string&mode,const std::string&attack="",const std::string&protocol_suite=crypto::suite,Profile=Profile::Default,int audit_fd=-1);
 bool prover()const override{return prover_;}std::string session()const override{return sid_;}
 Bytes message(const std::string&,const Bytes& = {})override;Fr coin(const std::string&)override;
 Digest prefix()const override{return transcript_.digest();}
 std::uint64_t proofBytes()const{return sent_;}
 std::uint64_t wireBytes()const{return total_;}std::uint64_t frames()const{return seq_;}std::uint64_t maxFrame()const{return max_frame_;}
 const std::string& attack()const{return attack_;}
 const Bytes& publicTranscript()const{return public_transcript_;}
};
}
