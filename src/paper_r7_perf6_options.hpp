#pragma once
#include "paper_r7_crypto.hpp"
#include <cstdlib>
#include <string>
namespace zkgpt::r7::perf6 {
// Read per invocation: no stale cached environment configuration.
inline bool enabled(const char*name){
 const auto*value=std::getenv(name);if(!value)return true;
 const std::string text(value);
 crypto::need(text=="0" || text=="1",(std::string(name)+" must be 0 or 1").c_str());
 return text=="1";
}
struct Options {bool a2,c2,b2;};
inline Options options(){return {enabled("ZKGPT_PERF6_A2"),enabled("ZKGPT_PERF6_C2"),enabled("ZKGPT_PERF6_B2")};}
// Stack scoped; never retains witness data or protocol state.
inline thread_local bool messages=false;
class ScopedMessages {
 bool previous_;
 public:explicit ScopedMessages(bool active):previous_(messages){messages=active;}
 ~ScopedMessages(){messages=previous_;}
 ScopedMessages(const ScopedMessages&)=delete;
 ScopedMessages&operator=(const ScopedMessages&)=delete;
};
}
