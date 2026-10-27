#pragma once
#include "fixed_cw.hpp"
namespace zkgpt::r7::stage2 {
// Private cache, never serialized to the protocol wire or public artifacts.
struct PrivateFixture{Registry primary,alternate,bad_tail;PCSecret weight,alternate_weight,bad_tail_weight;std::vector<Fr>activation;Fr output=0;};
Bytes encodePrivateFixture(const PrivateFixture&);PrivateFixture decodePrivateFixture(const Bytes&);
}
