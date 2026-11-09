#pragma once
namespace zkgpt::r7::perf6 {
// Stack-scoped opt-in experiment. Never retains witness data or protocol state.
inline thread_local bool messages=false;
class ScopedMessages {
 bool previous_;
 public:explicit ScopedMessages(bool enabled):previous_(messages){messages=enabled;}
 ~ScopedMessages(){messages=previous_;}
 ScopedMessages(const ScopedMessages&)=delete;
 ScopedMessages&operator=(const ScopedMessages&)=delete;
};
}
