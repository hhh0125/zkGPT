#include "zkgpt_artifact_io.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>

std::vector<std::uint8_t> readZkGPTArtifactFile(
    const std::string &path, std::size_t maximum_bytes,
    const char *artifact_kind) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if (!input) throw std::runtime_error("cannot open "+path);
    const std::streamsize signed_size=input.tellg();
    if (signed_size<0) throw std::runtime_error("cannot size "+path);
    const auto unsigned_size=static_cast<std::uintmax_t>(signed_size);
    if (unsigned_size>maximum_bytes)
        throw std::length_error(std::string(artifact_kind)+
            " file exceeds deployment size limit");
    if (unsigned_size>std::numeric_limits<std::size_t>::max())
        throw std::length_error(std::string(artifact_kind)+
            " file size exceeds addressable memory");
    const auto size=static_cast<std::size_t>(unsigned_size);
    std::vector<std::uint8_t> bytes(size);
    input.seekg(0);
    if (size!=0 && !input.read(reinterpret_cast<char *>(bytes.data()),
                               static_cast<std::streamsize>(size)))
        throw std::runtime_error("cannot read "+path);
    return bytes;
}
