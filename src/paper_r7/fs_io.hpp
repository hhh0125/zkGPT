#pragma once
#include "crypto.hpp"
#include <memory>
namespace zkgpt::r7::fs {
using namespace crypto;
inline constexpr const char* suite="R7-CLASSIFIED-HYRAX-LOOKUP-FS-ROWMAJOR-CANDIDATE-v3";
inline constexpr const char* full12Suite="R7-FULL12-CLASSIFIED-HYRAX-LOOKUP-FS-ROWMAJOR-CANDIDATE-v2";
inline constexpr const char* unifiedSuite="R7-ONEBLOCK-PROTOCOL1-UNIFIED-HIDDEN-FS-CANDIDATE-v1";
inline constexpr const char* unifiedFull12Suite="R7-FULL12-PROTOCOL1-UNIFIED-HIDDEN-FS-CANDIDATE-v1";
inline constexpr const char* hashSuite="SHA256-REJECT-LE32-NONZERO-v1";
inline constexpr const char* verifiedStatus="FS_NIZK_CANDIDATE_VERIFIED";
struct Binding {std::string proof_type,context,parameter_id;Bytes public_instance;};
std::string generatorIdentity(const std::vector<G1>& trusted_extra={});
class CanonicalTranscript {
 struct Impl;std::unique_ptr<Impl>impl_;
 public:CanonicalTranscript(const Bytes&header,const Bytes&public_instance,bool full12=false);~CanonicalTranscript();
 CanonicalTranscript(const CanonicalTranscript&)=delete;CanonicalTranscript&operator=(const CanonicalTranscript&)=delete;
 void message(const std::string&,const Bytes&);Fr coin(const std::string&);
 Digest prefix()const;Digest challengeDigest()const;
 std::uint64_t events()const;std::uint64_t messages()const;std::uint64_t coins()const;std::uint64_t rejections()const;double seconds()const;
};
class FsIO:public IO {
 struct Impl;std::unique_ptr<Impl>impl_;
 protected:FsIO(bool prover,const std::string&path,const Binding&);
 public:~FsIO()override;FsIO(const FsIO&)=delete;FsIO&operator=(const FsIO&)=delete;
 bool prover()const override;std::string session()const override;
 Bytes message(const std::string&,const Bytes& = {})override;Fr coin(const std::string&)override;Digest prefix()const override;
 void finish();std::uint64_t frames()const;std::uint64_t wireBytes()const;std::uint64_t proofBytes()const;
 std::string metadataJson()const;
};
class FsProverIO final:public FsIO {public:FsProverIO(const std::string&p,const Binding&b):FsIO(true,p,b){}};
class FsVerifierIO final:public FsIO {public:FsVerifierIO(const std::string&p,const Binding&b):FsIO(false,p,b){}};
}
