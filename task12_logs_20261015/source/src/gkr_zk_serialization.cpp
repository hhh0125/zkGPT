#include "gkr_serialization.hpp"
#include "circuit.h"
#include <algorithm>
#include <stdexcept>

namespace {
using namespace zkgpt::zk;
constexpr std::uint64_t kProofMagic=0x31505a524b475aULL,kStatementMagic=0x31535a524b475aULL;
constexpr std::size_t kMaxProofBytes=128ULL<<20,kMaxStatementBytes=2ULL<<20;
void circuitBounds(const layeredCircuit& c) {
    if (c.size<2 || c.size>4096 || c.size!=c.circuit.size())
        throw std::invalid_argument("ZK GKR codec public circuit size");
    for (const auto& layer:c.circuit)
        if (layer.bit_length<0 || layer.bit_length>30 || layer.max_bl_u<0 || layer.max_bl_u>30 ||
            layer.max_bl_v<0 || layer.max_bl_v>30)
            throw std::invalid_argument("ZK GKR codec public circuit dimensions");
}
std::size_t normalRounds(const layer& current) {
    return current.max_bl_u+(current.need_phase2?current.max_bl_v:0);
}
void writeMask(Writer& w,const MaskedSumcheckProof& p,std::size_t L) {
    w.bytes(encodeMaskedSumcheck(p,L,2));
}
MaskedSumcheckProof readMask(Reader& r,std::size_t L) {
    return decodeMaskedSumcheck(r.bytes(maskedSumcheckEncodedSize(L,2)),L,2);
}
void writeMatrix(Writer& w,const GKRZkMatrixProof& p) {
    const auto L=p.masked.rounds.size();
    if (L>30)throw std::invalid_argument("ZK matrix codec round bound");
    w.u64(L);writeMask(w,p.masked,L);w.point(p.f_terminal);w.point(p.g_terminal);
    w.point(p.product_commitment);w.bytes(encode(p.terminal_knowledge));w.bytes(encode(p.product));
    if (p.terminal_knowledge.A.size()!=2)throw std::invalid_argument("ZK matrix endpoint count");
}
GKRZkMatrixProof readMatrix(Reader& r) {
    const auto L=r.u64();
    if (L>30)throw std::invalid_argument("ZK matrix codec round bound");
    GKRZkMatrixProof p;p.masked=readMask(r,L);p.f_terminal=r.point();p.g_terminal=r.point();
    p.product_commitment=r.point();p.terminal_knowledge=decodeRepresentation(r.bytes(8+96*2),2);
    p.product=decodeProduct(r.bytes(160));return p;
}
bool failed(std::string* error,const std::exception& e) {if(error)*error=e.what();return false;}
} // namespace

zkgpt::zk::Bytes serializeGKRZkProof(const layeredCircuit& c,const GKRZkProof& proof) {
    circuitBounds(c);
    if (proof.layers.size()!=c.size-1)throw std::invalid_argument("ZK GKR codec layer count");
    Writer w(kMaxProofBytes);w.u64(kProofMagic);w.u64(kGKRZkVersion);w.u64(proof.layers.size());
    w.fr(proof.output_evaluation);
    for (std::size_t i=0;i<proof.layers.size();++i) {
        const auto id=c.size-1-static_cast<std::uint32_t>(i);const auto& current=c.circuit[id];
        const auto& p=proof.layers[i];
        if (p.layer_index!=id || p.layer_type!=static_cast<std::uint32_t>(current.ty))
            throw std::invalid_argument("ZK GKR codec layer metadata");
        w.u64(p.layer_index);w.u64(p.layer_type);
        if (current.ty==layerType::FCONN) {
            if (p.terminals[0]!=p.matrix_u.f_terminal || p.terminals[2]!=p.matrix_u.g_terminal ||
                !p.terminals[1].isZero() || !p.terminals[3].isZero())
                throw std::invalid_argument("ZK GKR codec matrix shared claims");
            writeMatrix(w,p.matrix_u);writeMatrix(w,p.matrix_v);
        } else {
            writeMask(w,p.normal_mask,normalRounds(current));
            for (const auto& terminal:p.terminals)w.point(terminal);
            if (p.terminal_knowledge.A.size()!=4)throw std::invalid_argument("ZK normal endpoint count");
            w.bytes(encode(p.terminal_knowledge));
            for (std::size_t j=0;j<3;++j) {w.point(p.products[j]);w.bytes(encode(p.product_relations[j]));}
        }
    }
    writeMask(w,proof.lasso.masked,c.circuit[0].bit_length);
    w.point(proof.lasso.input_commitment);w.fr(proof.lasso.mapping_evaluation);
    const auto shape=matrixShape(std::size_t{1}<<c.circuit[0].bit_length);
    if (proof.lasso.input_opening.z.size()!=shape.columns)throw std::invalid_argument("ZK GKR codec opening dimensions");
    w.bytes(encode(proof.lasso.input_opening));w.fr(proof.transcript_binding);return w.finish();
}
bool deserializeGKRZkProof(const zkgpt::zk::Bytes& bytes,const layeredCircuit& c,GKRZkProof& output,std::string* error) {
    try {
        circuitBounds(c);
        if (bytes.size()>kMaxProofBytes)throw std::invalid_argument("ZK GKR proof codec size limit");
        Reader r(bytes,bytes.size());
        if (r.u64()!=kProofMagic || r.u64()!=kGKRZkVersion || r.u64()!=c.size-1)
            throw std::invalid_argument("ZK GKR proof codec header");
        GKRZkProof p;p.output_evaluation=r.fr();p.layers.reserve(c.size-1);
        for (std::size_t i=0;i<c.size-1;++i) {
            const auto id=c.size-1-static_cast<std::uint32_t>(i);const auto& current=c.circuit[id];
            GKRZkLayerProof layer;
            if (r.u64()!=id || r.u64()!=static_cast<std::uint32_t>(current.ty))
                throw std::invalid_argument("ZK GKR proof codec layer metadata");
            layer.layer_index=id;layer.layer_type=static_cast<std::uint32_t>(current.ty);
            if (current.ty==layerType::FCONN) {
                layer.matrix_u=readMatrix(r);layer.matrix_v=readMatrix(r);
                layer.terminals[0]=layer.matrix_u.f_terminal;layer.terminals[2]=layer.matrix_u.g_terminal;
                layer.terminals[1].clear();layer.terminals[3].clear();
            } else {
                layer.normal_mask=readMask(r,normalRounds(current));
                for (auto& terminal:layer.terminals)terminal=r.point();
                layer.terminal_knowledge=decodeRepresentation(r.bytes(8+96*4),4);
                for (std::size_t j=0;j<3;++j) {layer.products[j]=r.point();layer.product_relations[j]=decodeProduct(r.bytes(160));}
            }
            p.layers.push_back(std::move(layer));
        }
        p.lasso.masked=readMask(r,c.circuit[0].bit_length);p.lasso.input_commitment=r.point();
        p.lasso.mapping_evaluation=r.fr();const auto columns=matrixShape(std::size_t{1}<<c.circuit[0].bit_length).columns;
        p.lasso.input_opening=decodeLinear(r.bytes(8+32*(4+columns)),columns);p.transcript_binding=r.fr();
        r.finish();output=std::move(p);return true;
    }catch (const std::exception& e) {return failed(error,e);}
}
zkgpt::zk::Bytes serializeGKRZkStatement(const GKRZkPublicStatement& statement) {
    const auto& s=statement.arithmetic;
    if (s.val0_log_size<0 || s.val0_log_size>30 || statement.context.empty() ||
        statement.context.size()>kMaxContextBytes || s.val0_generator_domain.domain!="zkGPT/main")
        throw std::invalid_argument("ZK GKR statement dimensions/context/basis");
    const auto shape=matrixShape(std::size_t{1}<<s.val0_log_size);
    if (s.val0_commitment.size()!=shape.rows || s.val0_generator_domain.count!=shape.columns ||
        s.protocol_version!=kZkGPTProtocolVersion)throw std::invalid_argument("ZK GKR statement row/generator dimensions");
    Writer w(kMaxStatementBytes);w.u64(kStatementMagic);w.u64(kGKRZkVersion);
    const auto& m=s.model_shape;
    for (auto value:{m.sequence_length,m.layer_count,m.head_count,m.head_dimension,m.hidden_dimension,m.mlp_dimension})w.u64(value);
    w.u64(s.val0_log_size);w.u64(statement.context.size());
    w.bytes(Bytes(statement.context.begin(),statement.context.end()));
    w.bytes(Bytes(s.circuit_fingerprint.begin(),s.circuit_fingerprint.end()));w.fr(s.output_evaluation);
    for (const auto& row:s.val0_commitment)w.point(row);return w.finish();
}
bool deserializeGKRZkStatement(const zkgpt::zk::Bytes& bytes,GKRZkPublicStatement& output,std::string* error) {
    try {
        if (bytes.size()>kMaxStatementBytes)throw std::invalid_argument("ZK GKR statement codec size limit");
        Reader r(bytes,bytes.size());if (r.u64()!=kStatementMagic || r.u64()!=kGKRZkVersion)
            throw std::invalid_argument("ZK GKR statement codec header");
        GKRZkPublicStatement statement;auto& s=statement.arithmetic;auto& m=s.model_shape;
        m.sequence_length=r.u64();m.layer_count=r.u64();m.head_count=r.u64();m.head_dimension=r.u64();
        m.hidden_dimension=r.u64();m.mlp_dimension=r.u64();const auto L=r.u64();
        if (L>30)throw std::invalid_argument("ZK GKR statement input dimension limit");s.val0_log_size=static_cast<int>(L);
        const auto shape=matrixShape(std::size_t{1}<<L);s.val0_generator_domain={"zkGPT/main",static_cast<std::uint32_t>(shape.columns)};
        const auto context_size=r.u64();if (!context_size || context_size>kMaxContextBytes)
            throw std::invalid_argument("ZK GKR statement context limit");
        const auto context=r.bytes(context_size);statement.context.assign(context.begin(),context.end());
        const auto fingerprint=r.bytes(32);std::copy(fingerprint.begin(),fingerprint.end(),s.circuit_fingerprint.begin());
        s.output_evaluation=r.fr();s.val0_commitment.reserve(shape.rows);
        for (std::size_t i=0;i<shape.rows;++i)s.val0_commitment.push_back(r.point());
        r.finish();output=std::move(statement);return true;
    }catch (const std::exception& e) {return failed(error,e);}
}
