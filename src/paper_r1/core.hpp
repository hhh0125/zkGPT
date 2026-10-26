#pragma once
#include <mcl/bn256.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>
namespace zkgpt::r1 {
using mcl::bn::Fr;
using mcl::bn::G1;
using Bytes = std::vector<std::uint8_t>;
using Digest = std::array<std::uint8_t, 32>;
constexpr const char *kModulus =
    "16798108731015832284940804142231733909759579603404752749028378864165570215949";
constexpr std::size_t kMaxVector = std::size_t(1) << 28;
void require(bool ok, const std::string &why);
void init();
unsigned log2exact(std::size_t n);
Bytes encode(const Fr &x);
// Rechecks the actual object; no validation cache or mutable validated state.
bool isValidG1(const G1 &x);
Bytes encode(const G1 &x);
Fr decodeFr(const Bytes &b);
G1 decodeG1(const Bytes &b);
Digest hash(const Bytes &b);
class Fs {
    Digest state_{};
    std::uint64_t counter_ = 0;

  public:
    explicit Fs(const std::string &domain);
    void bytes(const std::string &label, const Bytes &value);
    void text(const std::string &label, const std::string &value);
    void integer(const std::string &label, std::uint64_t value);
    void scalar(const std::string &label, const Fr &value);
    void group(const std::string &label, const G1 &value);
    Fr challenge(const std::string &label, bool nonzero = false);
    Digest digest() const { return state_; }
};
std::vector<Fr> point(Fs &t, unsigned bits, const std::string &label);
std::vector<Fr> weights(const std::vector<Fr> &r);
Fr mle(std::vector<Fr> v, const std::vector<Fr> &r);
Fr equality(const std::vector<Fr> &r, const std::vector<Fr> &s);
Fr dot(const std::vector<Fr> &a, const std::vector<Fr> &b);
G1 msm(const std::vector<G1> &g, const std::vector<Fr> &a);
} // namespace zkgpt::r1
