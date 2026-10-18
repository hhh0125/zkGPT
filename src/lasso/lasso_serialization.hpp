#pragma once
#include "lasso_types.hpp"
namespace route_b::lasso {
class Writer {
  public:
    Bytes bytes;
    void u32(std::uint32_t v);
    void tag(std::uint32_t v) { u32(v); }
    void raw(const std::uint8_t *p, std::size_t n);
    void string(const std::string &s);
    void fr(const Fr &v);
    void g1(const G1 &v);
};
Bytes encodeInstance(const LassoLookupInstance &x);
LassoLookupInstance decodeInstance(const Bytes &bytes);
Bytes encodeProof(const LassoProof &p);
LassoProof decodeProof(const Bytes &bytes);
Bytes readFile(const std::string &path);
void writeFile(const std::string &path, const Bytes &bytes);
} // namespace route_b::lasso
