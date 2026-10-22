#pragma once
#include "core.hpp"
namespace zkgpt::r1 {
enum class Layout : unsigned { ColumnMajor = 0, RowMajor = 1 };
struct PCSpec {
    std::string role;
    std::size_t actual = 0, padded = 0, rows = 0, columns = 0;
    Layout layout = Layout::ColumnMajor;
    static PCSpec make(const std::string &role, std::size_t actual, std::size_t padded,
                       Layout layout);
    void validate() const;
    bool operator==(const PCSpec &b) const;
};
struct Generators {
    std::vector<G1> g;
    G1 u;
};
struct Commitment {
    PCSpec spec;
    std::vector<G1> rows;
};
struct IpaRound {
    G1 left, right;
};
struct OpenProof {
    Fr value = 0;
    std::vector<IpaRound> rounds;
    Fr final = 0;
};
const Generators &generators(const PCSpec &s);
void append(Fs &t, const PCSpec &s);
void append(Fs &t, const Commitment &c);
Commitment commit(const PCSpec &s, const std::vector<Fr> &w);
OpenProof open(const PCSpec &expected, const Commitment &c, const std::vector<Fr> &w,
               const std::vector<Fr> &r, Fs &t, const std::string &label);
void verify(const PCSpec &expected, const Commitment &c, const std::vector<Fr> &r,
            const OpenProof &p, Fs &t, const std::string &label);
} // namespace zkgpt::r1
