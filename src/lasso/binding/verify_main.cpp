#include "lasso_serialization.hpp"
#include "witness_binding.hpp"
#include <chrono>
#include <iostream>
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      throw std::runtime_error(
          "usage: lasso_binding_verify TRUSTED.instance artifact.b2");
    mcl::bn::initPairing(mcl::BN254);
    auto begin = std::chrono::steady_clock::now();
    using namespace route_b::binding;
    auto x = decodeBindingInstance(readFile(argv[1]));
    auto p = decodeBindingProof(readFile(argv[2]));
    std::string e;
    auto c = verifyBinding(x, p, &e);
    if (!c)
      throw std::runtime_error(e);
    std::cout << "ACCEPT verified_witness_binding_claim m=" << c->query_length
              << " verify_ms="
              << std::chrono::duration<double, std::milli>(
                     std::chrono::steady_clock::now() - begin)
                     .count()
              << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "REJECT " << e.what() << '\n';
    return 1;
  }
}
