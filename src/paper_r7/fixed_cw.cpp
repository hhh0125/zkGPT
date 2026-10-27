#include "fixed_cw.hpp"
#include "paper_r2/r5b_trust.hpp"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
namespace zkgpt::r7::stage2 {
namespace {
std::string generatorIdentity(std::size_t columns){auto p=parameters(columns);Writer w;w.text(crypto::suite);w.point(p.scalar);w.point(p.hiding);for(auto&g:p.columns)w.point(g);return hex(r1::hash(w.b));}
void validateRegistry(const Registry&r){
 validate(r.pc);need(r.proof_suite==hyrax_a3::suite && r.curve=="MCL-BN254-y2=x3+2-Fr-v1" && r.generators==generatorIdentity(r.pc.spec.columns),"S2 registry suite/generator mismatch");
 need(r.profile==r5b::profileHash() && (r.id==componentId || r.id==std::string(componentId)+"/alternate") && r.layout=="R6-Layer4-FC0-prefix768-zero-tail256-column-major","S2 registry profile/object/layout mismatch");
 need(r.logical==768 && r.shape_rows==1 && r.shape_columns==768 && r.pc.spec.role==r.id,"S2 registry shape/object mismatch");
 need(r.pc.spec.actual==1024 && r.pc.spec.padded==1024,"S2 registry zero-tail domain mismatch");
 need(r.registry_id.size()==64 && r.source_manifest==manifestPin && r.circuit=="1cf3bc83a9a9f289e540238255d467b7fa6e2801209aa00f07fe232c48635a9a","S2 registry authority/profile provenance identity mismatch");
}
}
Registry registry(const PC&pc){Registry r;r.pc=pc;r.id=pc.spec.role;r.profile=r5b::profileHash();r.generators=generatorIdentity(pc.spec.columns);r.registry_id=hex(pcIdentity(pc));validateRegistry(r);return r;}
Bytes encodeRegistry(const Registry&r){validateRegistry(r);Writer w;w.text(version);w.text(crypto::suite);w.text(r.proof_suite);w.text(r.curve);w.text(r.generators);w.text(r.profile);w.text(r.id);w.text(r.layout);w.number(r.logical);w.number(r.shape_rows);w.number(r.shape_columns);auto p=encode(r.pc);w.number(p.size());w.raw(p);auto d=pcIdentity(r.pc);w.raw(Bytes(d.begin(),d.end()));w.text(r.registry_id);w.text(r.source_manifest);w.text(r.circuit);return w.b;}
Registry decodeRegistry(const Bytes&b){Reader r{b};need(r.text()==version && r.text()==crypto::suite,"S2 registry version/commitment suite mismatch");Registry out;out.proof_suite=r.text();out.curve=r.text();out.generators=r.text();out.profile=r.text();out.id=r.text();out.layout=r.text();out.logical=r.number();out.shape_rows=r.number();out.shape_columns=r.number();out.pc=decodePC(r.raw(r.number()));auto digest=r.raw(32);out.registry_id=r.text();out.source_manifest=r.text();out.circuit=r.text();r.end();auto expected=pcIdentity(out.pc);need(digest==Bytes(expected.begin(),expected.end()),"S2 registry commitment digest mismatch");validateRegistry(out);need(encodeRegistry(out)==b,"S2 noncanonical registry");return out;}
std::string registryDigest(const Registry&r){return hex(r1::hash(encodeRegistry(r)));}
Bytes readOwnedFD(int fd,bool secret){
 struct stat st{};need(fd>2 && fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==getuid() && st.st_size>0 && st.st_size<=1024*1024,"S2 trusted fixture FD invalid");
 need(!secret || (st.st_mode&077)==0,"S2 private fixture permissions");auto flags=fcntl(fd,F_GETFL);need(flags>=0 && (flags&O_ACCMODE)==O_RDONLY,"S2 fixture must be read-only");
 Bytes b(st.st_size);std::size_t at=0;while(at<b.size()){auto n=pread(fd,b.data()+at,b.size()-at,at);if(n<0 && errno==EINTR)continue;need(n>0,"S2 fixture read failed");at+=std::size_t(n);}return b;
}
void writeOwnedFD(int fd,const Bytes&b,bool secret){struct stat st{};need(fd>2 && fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==getuid() && st.st_size==0,"S2 exclusive fixture output FD invalid");need(!secret || (st.st_mode&077)==0,"S2 private output permissions");std::size_t at=0;while(at<b.size()){auto n=write(fd,b.data()+at,b.size()-at);if(n<0 && errno==EINTR)continue;need(n>0,"S2 fixture write failed");at+=std::size_t(n);}}
TrustedRegistry TrustedRegistry::load(int fd,const std::string&digest){auto b=readOwnedFD(fd);need(digest.size()==64 && hex(r1::hash(b))==digest,"S2 untrusted registry source/digest");auto r=decodeRegistry(b);need(r.id==componentId,"S2 approved registry object is not active component");return TrustedRegistry(std::move(r));}
void registration(IO&io,const Registry&approved,const PCSecret*secret,Random*rng){
 validateRegistry(approved);auto canonical=encodeRegistry(approved);need(io.message("S2/registry/approved",canonical)==canonical,"S2 unapproved registry identity/rebinding");
 if(io.prover())need(secret && rng && secret->values.size()==approved.pc.spec.padded && secret->blinds.size()==approved.pc.rows.size(),"S2 registration private shape");
 const auto p=parameters(approved.pc.spec.columns);need(approved.logical%approved.pc.spec.rows==0,"S2 unaligned zero-tail INCOMPLETE");const auto active=approved.logical/approved.pc.spec.rows;std::vector<G1>bases(p.columns.begin(),p.columns.begin()+active);std::vector<std::vector<Fr>>d;std::vector<Fr>rho;Writer mask;
 // Constrained representation: omitted columns MUST be zero. The row PoK
 // extracts only logical-prefix coordinates and h randomness, not padded data.
 if(io.prover())for(std::size_t i=0;i<approved.pc.spec.rows;++i){std::vector<Fr>row;for(std::size_t j=0;j<active;++j)row.push_back(rng->scalar());d.push_back(row);rho.push_back(rng->scalar());mask.point(msm(bases,row)+p.hiding*rho.back());}
 auto b=io.message("S2/registry/row-masks",mask.b);Reader mr{b};std::vector<G1>M;for(std::size_t i=0;i<approved.pc.spec.rows;++i)M.push_back(mr.point());mr.end();auto c=io.coin("S2/registry/knowledge-c");need(!c.isZero(),"S2 zero registration challenge");Writer response;
 if(io.prover())for(std::size_t i=0;i<approved.pc.spec.rows;++i){for(std::size_t j=0;j<active;++j)response.scalar(d[i][j]+c*secret->values[i+approved.pc.spec.rows*j]);response.scalar(rho[i]+c*secret->blinds[i]);}
 b=io.message("S2/registry/row-responses",response.b);Reader rr{b};for(std::size_t i=0;i<approved.pc.spec.rows;++i){std::vector<Fr>z;for(std::size_t j=0;j<active;++j)z.push_back(rr.scalar());auto blind=rr.scalar();if(!io.prover())need(msm(bases,z)+p.hiding*blind==M[i]+approved.pc.rows[i]*c,"S2 registry row knowledge equation failed");}rr.end();
}
}
