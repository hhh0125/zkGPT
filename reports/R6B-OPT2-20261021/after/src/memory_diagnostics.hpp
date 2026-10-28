#pragma once

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

class layeredCircuit;

namespace zkgpt::memory_diagnostics {

// Diagnostics are deliberately opt-in. They only inspect process memory and
// container metadata; none of the values are fed into a proof or transcript.
bool enabled();
inline void logAddress(const std::string&stage,const std::string&name,const void*data,
                       std::size_t bytes,const char*owner){
 if(enabled())std::fprintf(stderr,"[ZKGPT_ADDR] stage=%s object=%s address=%p bytes=%zu owner=%s\n",stage.c_str(),name.c_str(),const_cast<void*>(data),bytes,owner);
}

void logProcess(const std::string &stage);
void logAllocation(const std::string &stage, const std::string &name,
                   std::size_t count, std::size_t element_size);
void logScalar(const std::string &stage, const std::string &name,
               std::size_t value);
void logCircuitStorage(const std::string &stage, const layeredCircuit &circuit);
void logCircuit(const std::string &stage, const layeredCircuit &circuit);
// Linux mincore reports current resident pages, not cumulative accesses or
// exclusive physical ownership. It never faults the allocation's pages in.
void logResidentPages(const std::string &stage, const std::string &name,
                      const void *data, std::size_t bytes);
void logVectorMetadata(const std::string &stage, const std::string &name,
                       std::size_t size, std::size_t capacity,
                       std::size_t element_size);

template <typename T>
void logVector(const std::string &stage, const std::string &name,
               const std::vector<T> &values) {
    if (!enabled()) return;
    logVectorMetadata(stage, name, values.size(), values.capacity(), sizeof(T));
}

}  // namespace zkgpt::memory_diagnostics
