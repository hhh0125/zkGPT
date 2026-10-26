#include "interactive.hpp"
#include <unistd.h>
#include <cerrno>
#include <algorithm>
namespace zkgpt::r7::crypto {
namespace {
void exact(int fd,std::uint8_t*data,std::size_t n,bool write){std::size_t at=0;while(at<n){auto k=write ? ::write(fd,data+at,n-at) : ::read(fd,data+at,n-at);if(k<0 && errno==EINTR)continue;need(k>0,"R7 interactive EOF/transport failure");at+=std::size_t(k);}}
Bytes readFrame(){Bytes head(4);exact(0,head.data(),4,false);Reader r{head};auto n=r.number();need(n && n<=4*1024*1024,"R7 interactive length bound");Bytes b(n);exact(0,b.data(),b.size(),false);return b;}
}
PipeIO::PipeIO(bool p,const std::string&mode,const std::string&attack):prover_(p),attack_(attack){
 if(!p){auto b=coins_.bytes(32);Digest d;std::copy(b.begin(),b.end(),d.begin());sid_=hex(d);send("READY/"+mode,Bytes(suite,suite+std::char_traits<char>::length(suite)));}
 else{auto b=readFrame();auto r=R7Wire::decodeLocalRecord(b);need(r.sequence==0 && r.label=="READY/"+mode && r.payload==Bytes(suite,suite+std::char_traits<char>::length(suite)),"R7 trusted verifier READY mismatch");sid_=r.session;++seq_;transcript_.record(r.label,b);}
}
void PipeIO::send(const std::string&label,const Bytes&payload){need(seq_<4096 && total_<=16*1024*1024,"R7 interactive budget");R7Wire::LocalRecord record{sid_,label,seq_,payload};
 if(prover_ && !attacked_ && (attack_=="order" || attack_=="session" || attack_=="receipt")){if(attack_=="session")record.session=std::string(64,'0');else record.label=attack_=="receipt"?"import-prover-receipt":"wrong-order";attacked_=true;}
 auto b=R7Wire::encodeLocalRecord(record);Writer w;w.number(b.size());w.raw(b);exact(1,w.b.data(),w.b.size(),true);total_+=w.b.size();if(prover_)sent_+=w.b.size();++seq_;transcript_.record(label,b);
}
Bytes PipeIO::receive(const std::string&label){need(seq_<4096 && total_<=16*1024*1024,"R7 interactive budget");auto b=readFrame();total_+=b.size()+4;auto r=R7Wire::decodeLocalRecord(b);need(r.session==sid_,"R7 interactive cross-session rejected");need(r.sequence==seq_ && r.label==label,"R7 interactive sequence/label or receipt rejected");++seq_;transcript_.record(label,b);return r.payload;}
Bytes PipeIO::message(const std::string&label,const Bytes&value){if(prover_){auto b=value;if(attack_=="blind-response" && !attacked_ && label.find("/responses")!=std::string::npos && b.size()>=32){auto last=r1::decodeFr(Bytes(b.end()-32,b.end()));auto changed=r1::encode(last+Fr(1));std::copy(changed.begin(),changed.end(),b.end()-32);attacked_=true;}send(label,b);return b;}return receive(label);}
Fr PipeIO::coin(const std::string&label){Fr c;if(!prover_){c=coins_.scalar(true);send("coin/"+label,r1::encode(c));}else{auto b=receive("coin/"+label);c=r1::decodeFr(b);if(attack_=="challenge" && !attacked_){c+=Fr(1);attacked_=true;}}need(!c.isZero(),"R7 interactive zero challenge");return c;}
}
