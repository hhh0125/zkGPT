#include "model_binding.hpp"
#include "models.hpp"
#include "range_serialization.hpp"
#include "zkgpt_artifact_io.hpp"
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <unistd.h>
void writeExclusive(const std::string &p, const std::vector<uint8_t> &b) {
  int f = open(p.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (f < 0)
    throw std::runtime_error("cannot exclusively create " + p);
  size_t n = 0;
  while (n < b.size()) {
    auto k = write(f, b.data() + n, b.size() - n);
    if (k <= 0) {
      close(f);
      throw std::runtime_error("write failed");
    }
    n += k;
  }
  fsync(f);
  close(f);
}
auto read(const std::string &p) {
  return readZkGPTArtifactFile(p, 64ULL << 20, "binding");
}
int main(int argc, char **argv) {
  try {
    initPairing(mcl::BN254);
    if (argc == 3 && std::string(argv[1]) == "inspect") {
      auto a = decodeModelAnchor(read(argv[2]));
      if (a.profile_id != quantizationImplementationProfile())
        throw std::runtime_error("publisher profile differs from this build");
      size_t count = 0;
      for (auto &r : a.parameters)
        count += r.rows * r.cols;
      std::cout << "Trusted model_id=" << circuitFingerprintHex(a.model_id)
                << " profile_id=" << circuitFingerprintHex(a.profile_id)
                << " tensors=" << a.parameters.size()
                << " parameter_count=" << count << "\n";
      return 0;
    }

    if (argc == 5 && std::string(argv[1]) == "verify") {
      auto a = decodeModelAnchor(read(argv[2]));
      auto b = decodeBoundArtifact(read(argv[3]));
      auto x = readPublicEmbedding(argv[4]);
      std::string e;
      if (!verifyBoundArtifact(a, b, x, &e))
        throw std::runtime_error(e);
      std::cout << "Strict v4 model/input + Range/GKR + exp lookup ACCEPT\n";
      return 0;
    }
    if (argc >= 4 && std::string(argv[1]) == "prepare") {
      int depth = argc == 5 ? std::stoi(argv[4]) : 12;
      setenv("ZKGPT_PUBLIC_EMBEDDING", argv[3], 1);
      LLM n(depth, 12, 64, 768, 3072);
      prover p;
      n.create(p, false);
      auto a = prepareModelAnchor(n, p, quantizationImplementationProfile());
      writeExclusive(argv[2], encodeModelAnchor(a));
      std::cout << "Trusted publisher anchor model_id="
                << circuitFingerprintHex(a.model_id) << "\n";
      return 0;
    }
    throw std::runtime_error("usage: model_binding prepare ANCHOR EMBEDDING "
                             "[DEPTH] | verify ANCHOR ARTIFACT EMBEDDING");
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
