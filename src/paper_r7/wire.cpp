#include "wire.hpp"
#include "crypto.hpp"
#include <algorithm>
namespace zkgpt::r7 {
namespace {
void put(Bytes&b,std::uint64_t n,unsigned width){for(unsigned i=0;i<width;++i)b.push_back(std::uint8_t(n>>(8*i)));}
void text(Bytes&b,const std::string&s){r1::require(s.size()<=256,"R7 string bound");put(b,s.size(),4);b.insert(b.end(),s.begin(),s.end());}
struct Reader {
 const Bytes&b;std::size_t at=0;
 Bytes take(std::size_t n){r1::require(at<=b.size() && n<=b.size()-at,"R7 truncated wire");Bytes out(b.begin()+at,b.begin()+at+n);at+=n;return out;}
 std::uint64_t number(unsigned n){auto x=take(n);std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t(x[i])<<(8*i);return v;}
 std::string str(){auto n=number(4);r1::require(n<=256,"R7 string bound");auto x=take(n);return {x.begin(),x.end()};}
 void end(){r1::require(at==b.size(),"R7 trailing bytes");}
};
void values(Bytes&b,const std::vector<Fr>&v){r1::require(v.size()==32768,"R7 complete public X/Y required");put(b,v.size(),4);for(auto&f:v){auto e=r1::encode(f);b.insert(b.end(),e.begin(),e.end());}}
std::vector<Fr> values(Reader&r){r1::require(r.number(4)==32768,"R7 public vector bound");std::vector<Fr> v;v.reserve(32768);for(unsigned i=0;i<32768;++i)v.push_back(r1::decodeFr(r.take(32)));return v;}
}
Bytes R7Wire::encodeStatement(const R7Statement&s){
 Bytes b; text(b,"ZKGPT-R7-STATEMENT");put(b,s.version,4);put(b,s.blocks,4);
 for(auto*p:{&s.profile_sha256,&s.circuit_id,&s.layout_id,&s.layout_sha256,&s.model_manifest_sha256,&s.output_semantics,&s.commitment_suite})text(b,*p);
 put(b,s.master_actual,8);put(b,s.master_padded,8);values(b,s.x);values(b,s.y);
 r1::require(b.size()<=maxStatementBytes,"R7 statement size bound");return b;
}
R7Statement R7Wire::decodeStatement(const Bytes&b){
 r1::require(b.size()<=maxStatementBytes,"R7 statement size bound");Reader r{b};r1::require(r.str()=="ZKGPT-R7-STATEMENT","R7 statement magic");
 R7Statement s;s.version=r.number(4);s.blocks=r.number(4);
 for(auto*p:{&s.profile_sha256,&s.circuit_id,&s.layout_id,&s.layout_sha256,&s.model_manifest_sha256,&s.output_semantics,&s.commitment_suite})*p=r.str();
 s.master_actual=r.number(8);s.master_padded=r.number(8);s.x=values(r);s.y=values(r);r.end();return s;
}
Bytes R7Wire::encodeProof(const R7Proof&p){
 Bytes b;text(b,"ZKGPT-R7-INCOMPLETE-ENVELOPE-v1");b.insert(b.end(),p.statement_digest.begin(),p.statement_digest.end());
 for(auto&part:p.parts){r1::require(!part || (!part->empty() && part->size()<=maxPartBytes),"R7 component size bound");put(b,part?part->size():0,4);if(part)b.insert(b.end(),part->begin(),part->end());}return b;
}
R7Proof R7Wire::decodeProof(const Bytes&b){
 r1::require(b.size()<=8*maxPartBytes+1024,"R7 proof size bound");Reader r{b};r1::require(r.str()=="ZKGPT-R7-INCOMPLETE-ENVELOPE-v1","R7 envelope version");
 R7Proof p;auto d=r.take(32);std::copy(d.begin(),d.end(),p.statement_digest.begin());
 for(auto&part:p.parts){auto n=r.number(4);r1::require(n<=maxPartBytes,"R7 component size bound");if(n)part=r.take(n);}r.end();return p;
}
R7Transcript::R7Transcript(const R7Statement&s):transcript_("zkGPT.paper-r7.round1.v1") {transcript_.bytes("statement",R7Wire::encodeStatement(s));}
R7Transcript::R7Transcript(const std::string&domain):transcript_(domain){transcript_.text("mode","honest-interactive-NOT-FIAT-SHAMIR");}
void R7Transcript::record(const std::string&label,const Bytes&b){r1::require(label.size()<=256,"R7 interactive label bound");transcript_.bytes(label,b);}
Bytes R7Wire::encodeLocalRecord(const LocalRecord&r){crypto::Writer w;w.text("R7-LOCAL-INTERACTIVE-v1");w.text(r.session);w.number(r.sequence,8);w.text(r.label);w.number(r.payload.size());w.raw(r.payload);return w.b;}
R7Wire::LocalRecord R7Wire::decodeLocalRecord(const Bytes&b){r1::require(b.size()<=4*1024*1024,"R7 interactive frame bound");crypto::Reader r{b};r1::require(r.text()=="R7-LOCAL-INTERACTIVE-v1","R7 interactive version");LocalRecord out;out.session=r.text();out.sequence=r.number(8);out.label=r.text();auto n=r.number();out.payload=r.raw(n);r.end();r1::require(out.session.size()==64,"R7 interactive session identity");return out;}
void R7Transcript::append(Component c,const Bytes&b){transcript_.bytes(std::string("R7.component/")+name(c),b);}
Fr R7Transcript::challenge(Component c,const std::string&label){return transcript_.challenge(std::string("R7.challenge/")+name(c)+"/"+label,true);}
}
