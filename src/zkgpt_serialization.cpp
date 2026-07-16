#include "zkgpt_serialization.hpp"

#include "gkr_serialization.hpp"
#include "range_serialization.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::uint32_t kVersion=1;
constexpr std::size_t kMaxArtifactBytes=1ULL<<30;
constexpr std::size_t kMaxCircuitIdBytes=128;

std::size_t frSize() {
    static const std::size_t size=[] {
        std::array<std::uint8_t,64> bytes{};
        Fr zero=0;
        const auto written=zero.serialize(bytes.data(),bytes.size());
        if (written==0) throw std::runtime_error("cannot size artifact Fr");
        return written;
    }();
    return size;
}

class Writer {
public:
    void raw(const void *data, std::size_t size) {
        if (size==0) return;
        if (size>kMaxArtifactBytes-bytes_.size())
            throw std::length_error("zkGPT artifact exceeds size limit");
        const auto *begin=static_cast<const std::uint8_t *>(data);
        if (!begin) throw std::invalid_argument("null artifact byte source");
        bytes_.reserve(bytes_.size()+size);
        bytes_.insert(bytes_.end(),begin,begin+size);
    }
    void u32(std::uint32_t value) {
        std::uint8_t bytes[4];
        for (unsigned i=0;i<4;++i)
            bytes[i]=static_cast<std::uint8_t>(value>>(8*i));
        raw(bytes,sizeof(bytes));
    }
    void size(std::size_t value) {
        if (value>std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("zkGPT artifact length exceeds uint32");
        u32(static_cast<std::uint32_t>(value));
    }
    void string(const std::string &value) {
        size(value.size());
        raw(value.data(),value.size());
    }
    void blob(const std::vector<std::uint8_t> &value) {
        size(value.size());
        raw(value.data(),value.size());
    }
    void shape(const WitnessShape &shape) {
        const std::size_t values[]={shape.sequence_length,shape.layer_count,
            shape.head_count,shape.head_dimension,shape.hidden_dimension,
            shape.mlp_dimension};
        for (const auto value : values) size(value);
    }
    void fr(const Fr &value) {
        std::array<std::uint8_t,64> bytes{};
        const auto written=value.serialize(bytes.data(),bytes.size());
        if (written!=frSize())
            throw std::runtime_error("non-canonical artifact Fr size");
        raw(bytes.data(),written);
    }
    std::vector<std::uint8_t> finish() { return std::move(bytes_); }
private:
    std::vector<std::uint8_t> bytes_;
};

class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t> &bytes):bytes_(bytes) {
        if (bytes.size()>kMaxArtifactBytes)
            throw std::length_error("zkGPT artifact exceeds size limit");
    }
    const std::uint8_t *take(std::size_t size) {
        if (size>bytes_.size()-position_)
            throw std::runtime_error("truncated zkGPT artifact");
        const auto *result=bytes_.data()+position_;
        position_+=size;
        return result;
    }
    std::uint32_t u32() {
        const auto *bytes=take(4);
        std::uint32_t result=0;
        for (unsigned i=0;i<4;++i)
            result|=static_cast<std::uint32_t>(bytes[i])<<(8*i);
        return result;
    }
    std::uint32_t length(std::uint32_t maximum,const char *label) {
        const auto value=u32();
        if (value>maximum)
            throw std::length_error(std::string(label)+" exceeds limit");
        return value;
    }
    std::string string() {
        const auto count=length(kMaxCircuitIdBytes,"circuit id length");
        const auto *bytes=take(count);
        return {reinterpret_cast<const char *>(bytes),count};
    }
    std::vector<std::uint8_t> blob() {
        const auto count=length(kMaxArtifactBytes,"nested artifact length");
        const auto *bytes=take(count);
        return {bytes,bytes+count};
    }
    WitnessShape shape() {
        WitnessShape value;
        value.sequence_length=u32();
        value.layer_count=u32();
        value.head_count=u32();
        value.head_dimension=u32();
        value.hidden_dimension=u32();
        value.mlp_dimension=u32();
        return value;
    }
    Fr fr() {
        const auto count=frSize();
        const auto *encoded=take(count);
        Fr value;
        if (value.deserialize(encoded,count)!=count)
            throw std::runtime_error("invalid artifact Fr encoding");
        std::array<std::uint8_t,64> canonical{};
        if (value.serialize(canonical.data(),canonical.size())!=count ||
            !std::equal(encoded,encoded+count,canonical.begin()))
            throw std::runtime_error("non-canonical artifact Fr encoding");
        return value;
    }
    void magic(const char expected[4]) {
        const auto *actual=take(4);
        if (!std::equal(actual,actual+4,
                        reinterpret_cast<const std::uint8_t *>(expected)))
            throw std::runtime_error("zkGPT artifact magic mismatch");
    }
    void finish() const {
        if (position_!=bytes_.size())
            throw std::runtime_error("trailing bytes after zkGPT artifact");
    }
private:
    const std::vector<std::uint8_t> &bytes_;
    std::size_t position_=0;
};

} // namespace

std::vector<std::uint8_t> serializeZkGPTPublicStatement(
    const ZkGPTPublicStatement &statement) {
    Writer writer;
    writer.raw("ZKGS",4);
    writer.u32(kVersion);
    writer.u32(statement.protocol_version);
    writer.string(statement.circuit_id);
    writer.shape(statement.model_shape);
    writer.raw(statement.circuit_fingerprint.data(),
               statement.circuit_fingerprint.size());
    writer.fr(statement.output_claim.mle_evaluation);
    writer.blob(serializeRangePublicStatement(statement.range_statement));
    return writer.finish();
}

bool deserializeZkGPTPublicStatement(
    const std::vector<std::uint8_t> &bytes,
    ZkGPTPublicStatement &statement, std::string *error) {
    try {
        Reader reader(bytes);
        reader.magic("ZKGS");
        if (reader.u32()!=kVersion)
            throw std::runtime_error("unsupported zkGPT statement version");
        ZkGPTPublicStatement decoded;
        decoded.protocol_version=reader.u32();
        decoded.circuit_id=reader.string();
        decoded.model_shape=reader.shape();
        const auto *fingerprint=reader.take(decoded.circuit_fingerprint.size());
        std::copy(fingerprint,
                  fingerprint+decoded.circuit_fingerprint.size(),
                  decoded.circuit_fingerprint.begin());
        decoded.output_claim.mle_evaluation=reader.fr();
        const auto range_bytes=reader.blob();
        std::string nested_error;
        if (!deserializeRangePublicStatement(
                range_bytes,decoded.range_statement,&nested_error))
            throw std::runtime_error("invalid nested Range statement: "+
                                     nested_error);
        reader.finish();
        statement=std::move(decoded);
        return true;
    } catch (const std::exception &exception) {
        if (error) *error=exception.what();
        return false;
    }
}

std::vector<std::uint8_t> serializeZkGPTProof(const ZkGPTProof &proof) {
    Writer writer;
    writer.raw("ZKPF",4);
    writer.u32(kVersion);
    writer.blob(serializeGKRProof(proof.gkr_proof));
    writer.blob(serializeRangeProof(proof.range_proof));
    writer.fr(proof.transcript_binding);
    return writer.finish();
}

bool deserializeZkGPTProof(const std::vector<std::uint8_t> &bytes,
                           ZkGPTProof &proof, std::string *error) {
    try {
        Reader reader(bytes);
        reader.magic("ZKPF");
        if (reader.u32()!=kVersion)
            throw std::runtime_error("unsupported zkGPT proof version");
        ZkGPTProof decoded;
        const auto gkr_bytes=reader.blob();
        const auto range_bytes=reader.blob();
        std::string nested_error;
        if (!deserializeGKRProof(gkr_bytes,decoded.gkr_proof,&nested_error))
            throw std::runtime_error("invalid nested GKR proof: "+nested_error);
        if (!deserializeRangeProof(
                range_bytes,decoded.range_proof,&nested_error))
            throw std::runtime_error("invalid nested Range proof: "+nested_error);
        decoded.transcript_binding=reader.fr();
        reader.finish();
        proof=std::move(decoded);
        return true;
    } catch (const std::exception &exception) {
        if (error) *error=exception.what();
        return false;
    }
}
