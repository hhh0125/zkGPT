#pragma once

#include "paper_r7_fs_io.hpp"
#include <filesystem>
#include <fstream>
#include <random>

namespace zkgpt::r7::test {
using namespace crypto;
struct SeededRandom final:Random {
 std::mt19937_64 engine{20261117};
 void fill(std::uint8_t*p,std::size_t n)override{for(std::size_t i=0;i<n;++i)p[i]=std::uint8_t(engine());}
};

// Test-only deterministic file prover. Uses the unchanged production header,
// event framing and CanonicalTranscript, and is checked by FsVerifierIO.
// Production FsProverIO continues to generate OS-random nonces.
struct FixedFixtureProver final:IO {
 std::unique_ptr<fs::CanonicalTranscript>transcript;Bytes file;std::string sid,path;
 FixedFixtureProver(const std::string&p,const fs::Binding&b):path(p){
  Writer h;h.text("ZKGPT-R7-FS-PROOF");h.number(1);h.text(fs::suite);h.text(fs::hashSuite);h.text("typed-u64le-events-v1");h.text(b.proof_type);h.text(b.context);h.text(b.parameter_id);
  auto digest=r1::hash(b.public_instance);h.raw(Bytes(digest.begin(),digest.end()));Bytes nonce(32);for(unsigned i=0;i<32;++i)nonce[i]=i+1;h.raw(nonce);
  transcript=std::make_unique<fs::CanonicalTranscript>(h.b,b.public_instance);
  const std::string label="zkGPT.R7.FS.session.v1";Bytes session(label.begin(),label.end());auto prefix=transcript->prefix();session.insert(session.end(),prefix.begin(),prefix.end());sid=hex(r1::hash(session));
  Writer start;start.raw(Bytes{'Z','K','R','7','F','S','1',0});start.number(h.b.size());start.raw(h.b);file=std::move(start.b);
 }
 bool prover()const override{return true;}std::string session()const override{return sid;}Digest prefix()const override{return transcript->prefix();}
 void frame(char tag,const std::string&label,const Bytes&b){Writer w;w.number(std::uint8_t(tag),1);w.number(transcript->events(),8);w.number(label.size(),8);w.raw(Bytes(label.begin(),label.end()));w.number(b.size(),8);w.raw(b);file.insert(file.end(),w.b.begin(),w.b.end());}
 Bytes message(const std::string&s,const Bytes&b={})override{frame('P',s,b);transcript->message(s,b);return b;}
 Fr coin(const std::string&s)override{return transcript->coin(s);}
 void finish(){Writer w;w.number(transcript->events(),8);w.number(transcript->messages(),8);w.number(transcript->coins(),8);auto a=transcript->prefix(),b=transcript->challengeDigest();w.raw(Bytes(a.begin(),a.end()));w.raw(Bytes(b.begin(),b.end()));frame('F',"end",w.b);transcript->finishTrace();need(!std::filesystem::exists(path),"fixture proof output already exists");std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(file.data()),file.size());out.close();need(bool(out),"fixture proof write");}
 std::uint64_t proofBytes()const{return file.size();}
};
}
