#include "generator_cache.hpp"

#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace {

using CacheKey=std::pair<std::string, std::uint32_t>;

std::map<CacheKey, GeneratorSet> &generatorCache() {
    static std::map<CacheKey, GeneratorSet> cache;
    return cache;
}

std::mutex &generatorCacheMutex() {
    static std::mutex mutex;
    return mutex;
}

void validateDescriptor(const GeneratorDomain &descriptor) {
    if (descriptor.domain!="zkGPT/main" &&
        descriptor.domain!="zkGPT/range")
        throw std::invalid_argument("unsupported generator domain");
    if (descriptor.count==0 || descriptor.count>kMaxGeneratorCount)
        throw std::invalid_argument("generator count is outside protocol limits");
}

}  // namespace

const GeneratorSet &getGeneratorSet(const GeneratorDomain &descriptor) {
    validateDescriptor(descriptor);
    std::lock_guard<std::mutex> lock(generatorCacheMutex());
    auto &cache=generatorCache();
    const CacheKey key{descriptor.domain, descriptor.count};
    const auto existing=cache.find(key);
    if (existing!=cache.end()) return existing->second;

    GeneratorSet generated;
    generated.generators.resize(descriptor.count);
    for (std::uint32_t i=0;i<descriptor.count;++i) {
        hashAndMapToG1(generated.generators[i],
                       descriptor.domain+"/g/"+std::to_string(i));
        if (!generated.generators[i].isValid() ||
            !generated.generators[i].isValidOrder() ||
            generated.generators[i].isZero())
            throw std::runtime_error("failed to derive protocol generator");
    }
    hashAndMapToG1(generated.u, descriptor.domain+"/u");
    if (!generated.u.isValid() || !generated.u.isValidOrder() ||
        generated.u.isZero())
        throw std::runtime_error("failed to derive protocol U generator");
    return cache.emplace(key, std::move(generated)).first->second;
}

