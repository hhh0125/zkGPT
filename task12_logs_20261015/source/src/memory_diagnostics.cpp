#include "memory_diagnostics.hpp"
#include "circuit.h"

#include <cstdlib>
#include <type_traits>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <cstdint>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

namespace zkgpt::memory_diagnostics {
namespace {

struct ProcessMemory {
    std::size_t vm_size_kib = 0;
    std::size_t vm_rss_kib = 0;
    std::size_t vm_hwm_kib = 0;
};

ProcessMemory readProcessMemory() {
    ProcessMemory result;
    std::ifstream status("/proc/self/status");
    std::string key;
    while (status >> key) {
        if (key == "VmSize:" || key == "VmRSS:" || key == "VmHWM:") {
            std::size_t value = 0;
            std::string unit;
            status >> value >> unit;
            if (key == "VmSize:") result.vm_size_kib = value;
            else if (key == "VmRSS:") result.vm_rss_kib = value;
            else result.vm_hwm_kib = value;
        } else {
            std::string ignored;
            std::getline(status, ignored);
        }
    }
    return result;
}

std::size_t capacityBytes(std::size_t count, std::size_t element_size) {
    if (element_size != 0 &&
        count > std::numeric_limits<std::size_t>::max() / element_size)
        return std::numeric_limits<std::size_t>::max();
    return count * element_size;
}

}  // namespace

bool enabled() {
    static const bool value = [] {
        const char *setting = std::getenv("ZKGPT_MEMORY_DIAGNOSTICS");
        return setting && std::string(setting) != "0" &&
               std::string(setting) != "false" &&
               std::string(setting) != "FALSE";
    }();
    return value;
}

void logProcess(const std::string &stage) {
    if (!enabled()) return;
    const ProcessMemory memory = readProcessMemory();
    struct rusage usage{};
    getrusage(RUSAGE_SELF,&usage);
    std::cerr << "[ZKGPT_MEM] stage=" << stage
              << " vmsize_kib=" << memory.vm_size_kib
              << " vmrss_kib=" << memory.vm_rss_kib
              << " vmhwm_kib=" << memory.vm_hwm_kib
              << " process_peak_rss_kib=" << usage.ru_maxrss << '\n';
}

void logResidentPages(const std::string &stage, const std::string &name,
                      const void *data, std::size_t bytes) {
    if (!enabled() || !data || !bytes) return;
    const long page_size=sysconf(_SC_PAGESIZE);
    if (page_size<=0) return;
    const std::size_t page=static_cast<std::size_t>(page_size);
    const std::uintptr_t address=reinterpret_cast<std::uintptr_t>(data);
    const std::uintptr_t base=address-address%page;
    const std::size_t span=bytes+(address-base);
    const std::size_t pages=span/page+(span%page!=0);
    std::vector<unsigned char> resident(pages);
    if (mincore(reinterpret_cast<void *>(base),span,resident.data())!=0) {
        std::cerr << "[ZKGPT_MEM] stage=" << stage << " object=" << name
                  << " mincore_unavailable=1\n";
        return;
    }
    std::size_t count=0;
    for (auto value : resident) count+=(value&1)!=0;
    std::cerr << "[ZKGPT_MEM] stage=" << stage << " object=" << name
              << " mapped_span_pages=" << pages
              << " resident_pages=" << count << " page_bytes=" << page
              << " resident_span_bytes=" << count*page << '\n';
}

void logAllocation(const std::string &stage, const std::string &name,
                   std::size_t count, std::size_t element_size) {
    if (!enabled()) return;
    std::cerr << "[ZKGPT_MEM] stage=" << stage
              << " object=" << name
              << " count=" << count
              << " element_size=" << element_size
              << " allocation_bytes=" << capacityBytes(count, element_size)
              << '\n';
}

void logCircuit(const std::string &stage, const layeredCircuit &circuit) {
    if (!enabled()) return;
    std::size_t us=0,uc=0,bs=0,bc=0,ms=0,mc=0,is=0,ic=0;
    for (const auto &layer : circuit.circuit) {
        us+=layer.uni_gates.size(); uc+=layer.uni_gates.capacity();
        bs+=layer.bin_gates.size(); bc+=layer.bin_gates.capacity();
        ms+=layer.ori_id_u.size()+layer.ori_id_v.size();
        mc+=layer.ori_id_u.capacity()+layer.ori_id_v.capacity();
        is+=layer.uni_interval.size()+layer.bin_interval.size();
        ic+=layer.uni_interval.capacity()+layer.bin_interval.capacity();
    }
    logVector(stage,"circuit.layers",circuit.circuit);
    logVectorMetadata(stage,"circuit.uni_gates",us,uc,sizeof(uniGate));
    logVectorMetadata(stage,"circuit.bin_gates",bs,bc,sizeof(binGate));
    logVectorMetadata(stage,"circuit.input_mapping",ms,mc,sizeof(u32));
    logVectorMetadata(stage,"circuit.intervals",is,ic,sizeof(std::pair<int,int>));
}

void logCircuitStorage(const std::string &stage, const layeredCircuit &circuit) {
    if (!enabled()) return;
    logProcess(stage);
    logCircuit(stage,circuit);
    for (std::size_t i=0;i<circuit.circuit.size();++i) {
        const auto &layer=circuit.circuit[i];
        const std::string prefix="layer_"+std::to_string(i)+".";
        logScalar(stage,prefix+"type",static_cast<std::size_t>(layer.ty));
        const auto inspect=[&](const std::string &name,const auto &values) {
            logVector(stage,prefix+name,values);
            using Element=typename std::decay_t<decltype(values)>::value_type;
            logAllocation(stage,prefix+name+".effective",values.size(),sizeof(Element));
            // mincore observes residency without touching reserve pages.
            logResidentPages(stage,prefix+name,values.data(),values.capacity()*sizeof(Element));
        };
        inspect("uni_gates",layer.uni_gates);
        inspect("bin_gates",layer.bin_gates);
        inspect("ori_id_u",layer.ori_id_u);
        inspect("ori_id_v",layer.ori_id_v);
    }
}

void logScalar(const std::string &stage, const std::string &name,
               std::size_t value) {
    if (!enabled()) return;
    std::cerr << "[ZKGPT_MEM] stage=" << stage
              << " metric=" << name
              << " value=" << value << '\n';
}

void logVectorMetadata(const std::string &stage, const std::string &name,
                       std::size_t size, std::size_t capacity,
                       std::size_t element_size) {
    if (!enabled()) return;
    std::cerr << "[ZKGPT_MEM] stage=" << stage
              << " object=" << name
              << " size=" << size
              << " capacity=" << capacity
              << " element_size=" << element_size
              << " capacity_bytes=" << capacityBytes(capacity, element_size)
              << '\n';
}

}  // namespace zkgpt::memory_diagnostics
