#pragma once
#include "framework.hpp"
namespace zkgpt::r7 {
struct R7Wire {
    struct LocalRecord { std::string session,label;std::uint64_t sequence=0;Bytes payload; };
    static Bytes encodeLocalRecord(const LocalRecord&);
    static LocalRecord decodeLocalRecord(const Bytes&);
    static constexpr std::size_t maxStatementBytes=2*1024*1024+8192;
    static constexpr std::size_t maxPartBytes=65536;
    static Bytes encodeStatement(const R7Statement&);
    static R7Statement decodeStatement(const Bytes&);
    static Bytes encodeProof(const R7Proof&);
    static R7Proof decodeProof(const Bytes&);
};
}
