#include "fs_io.hpp"
#include "hyrax_a3.hpp"
#include <openssl/evp.h>
#include <algorithm>
#include <chrono>
#include <set>
#include <cstring>
#include <sstream>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/fs.h>
#include <unistd.h>
#include <cerrno>
namespace zkgpt::r7::fs {
namespace {
constexpr std::uint64_t byteLimit=1ull<<30,eventLimit=1048576,payloadLimit=4ull<<20;
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
class Hash {
 std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>ctx_{EVP_MD_CTX_new(),EVP_MD_CTX_free};
 public:mutable double seconds=0;
 Hash(){need(ctx_ && EVP_DigestInit_ex(ctx_.get(),EVP_sha256(),nullptr)==1,"FS SHA256 init");}
 void add(const std::uint8_t*p,std::size_t n){auto t=Clock::now();need(EVP_DigestUpdate(ctx_.get(),p,n)==1,"FS SHA256 update");seconds+=elapsed(t);}
 void add(const Bytes&b){add(b.data(),b.size());}void text(const char*s){add(reinterpret_cast<const std::uint8_t*>(s),std::strlen(s));}
 Digest digest()const{auto t=Clock::now();std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>copy(EVP_MD_CTX_new(),EVP_MD_CTX_free);Digest d{};unsigned n=0;need(copy && EVP_MD_CTX_copy_ex(copy.get(),ctx_.get())==1 && EVP_DigestFinal_ex(copy.get(),d.data(),&n)==1 && n==32,"FS SHA256 snapshot");seconds+=elapsed(t);return d;}
};
Bytes bytes(const Digest&d){return {d.begin(),d.end()};}
Bytes eventHeader(char tag,std::uint64_t sequence,const std::string&label,std::uint64_t size){
 need(!label.empty() && label.size()<=256 && size<=payloadLimit,"FS event label/payload bound");Writer w;w.number(std::uint8_t(tag),1);w.number(sequence,8);w.number(label.size(),8);w.raw(Bytes(label.begin(),label.end()));w.number(size,8);return w.b;
}
Bytes digestBytes(const Bytes&b){Hash h;h.add(b);return bytes(h.digest());}
bool hexadecimal(const std::string&s){return s.size()==64 && std::all_of(s.begin(),s.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');});}
void validate(const Binding&b){need((b.proof_type=="Oneblock" || b.proof_type=="ClassifiedProbe" || b.proof_type=="Unit") && !b.context.empty() && b.context.size()<=256 && hexadecimal(b.parameter_id) && !b.public_instance.empty() && b.public_instance.size()<=payloadLimit,"FS independent public binding bounds");}
Bytes header(const Binding&b,const Bytes&nonce){Writer w;w.text("ZKGPT-R7-FS-PROOF");w.number(1);w.text(suite);w.text(hashSuite);w.text("typed-u64le-events-v1");w.text(b.proof_type);w.text(b.context);w.text(b.parameter_id);w.raw(digestBytes(b.public_instance));w.raw(nonce);return w.b;}
Bytes nonceFrom(const Bytes&h,const Binding&b){Reader r{h};need(r.text()=="ZKGPT-R7-FS-PROOF" && r.number()==1 && r.text()==suite && r.text()==hashSuite && r.text()=="typed-u64le-events-v1","FS unknown suite/version/framing; no downgrade");need(r.text()==b.proof_type && r.text()==b.context && r.text()==b.parameter_id && r.raw(32)==digestBytes(b.public_instance),"FS external statement/context/parameter identity mismatch");auto n=r.raw(32);r.end();need(std::any_of(n.begin(),n.end(),[](auto x){return x!=0;}),"FS zero proof nonce");need(header(b,n)==h,"FS noncanonical header");return n;}
}
struct CanonicalTranscript::Impl {
 Hash state,audit;std::set<std::pair<char,std::string>>labels;std::uint64_t seq=0,msg=0,coins=0,rejected=0;double candidate_seconds=0;
 Impl(const Bytes&h,const Bytes&i){state.text("zkGPT.R7.FS.canonical.SHA256.v1");audit.text("zkGPT.R7.FS.challenge-audit.v1");event('I',"header",h);event('I',"public-instance",i);}
 void event(char tag,const std::string&label,const Bytes&b){need(seq<eventLimit && labels.emplace(tag,label).second,"FS repeated event label/budget");state.add(eventHeader(tag,seq,label,b.size()));state.add(b);++seq;}
};
CanonicalTranscript::CanonicalTranscript(const Bytes&h,const Bytes&i):impl_(std::make_unique<Impl>(h,i)){}
CanonicalTranscript::~CanonicalTranscript()=default;
void CanonicalTranscript::message(const std::string&s,const Bytes&b){impl_->event('P',s,b);++impl_->msg;}
Fr CanonicalTranscript::coin(const std::string&s){auto&v=*impl_;need(!v.labels.count({'C',s}),"FS repeated challenge label");auto parent=v.state.digest();
 for(std::uint64_t counter=0;counter<4096;++counter){Writer w;w.raw(bytes(parent));w.number(v.coins,8);w.number(counter,8);Hash h;h.text(hashSuite);h.add(eventHeader('H',v.seq,s,w.b.size()));h.add(w.b);auto candidate=h.digest();v.candidate_seconds+=h.seconds;Fr f;
  if(f.deserialize(candidate.data(),candidate.size())==32 && f.isValid() && r1::encode(f)==bytes(candidate) && !f.isZero()){
   auto b=r1::encode(f);v.audit.add(eventHeader('C',v.seq,s,b.size()));v.audit.add(b);v.event('C',s,b);++v.coins;v.rejected+=counter;return f;
  }
 }throw std::runtime_error("FS nonzero field rejection budget exhausted");}
Digest CanonicalTranscript::prefix()const{return impl_->state.digest();}Digest CanonicalTranscript::challengeDigest()const{return impl_->audit.digest();}
std::uint64_t CanonicalTranscript::events()const{return impl_->seq;}std::uint64_t CanonicalTranscript::messages()const{return impl_->msg;}std::uint64_t CanonicalTranscript::coins()const{return impl_->coins;}std::uint64_t CanonicalTranscript::rejections()const{return impl_->rejected;}
double CanonicalTranscript::seconds()const{return impl_->state.seconds+impl_->audit.seconds+impl_->candidate_seconds;}
std::string generatorIdentity(const std::vector<G1>&extra){Writer w;w.text("R7-FS-MCL-BN254-original-generators-v1");w.text(crypto::suite);w.text(hyrax_a3::suite);w.text(hyrax_a3::trustedR6Suite);w.text(r1::kModulus);auto p=parameters(1);w.point(p.scalar);w.point(p.hiding);auto g=hyrax_a3::generators(1024);w.number(g.size());for(auto&x:g)w.point(x);w.number(extra.size());for(auto&x:extra)w.point(x);return hex(r1::hash(w.b));}
struct FsIO::Impl {
 bool prover,finished=false;int fd=-1,dir=-1;std::string final_name,temp_name,sid;std::unique_ptr<CanonicalTranscript>transcript;Hash file_hash;std::uint64_t total=0,max_payload=0;double io_seconds=0;struct stat original{};
 explicit Impl(bool p):prover(p){}~Impl(){if(fd>=0)::close(fd);if(dir>=0)::close(dir);}
 void transfer(std::uint8_t*p,std::size_t n){need(n<=byteLimit-total,"FS proof byte budget");auto t=Clock::now();std::size_t at=0;while(at<n){auto k=prover ? ::write(fd,p+at,n-at) : ::read(fd,p+at,n-at);if(k<0 && errno==EINTR)continue;need(k>0,"FS truncated proof or file IO failure");at+=std::size_t(k);}io_seconds+=elapsed(t);file_hash.add(p,n);total+=n;}
 void send(const Bytes&b){transfer(const_cast<std::uint8_t*>(b.data()),b.size());}Bytes receive(std::size_t n){need(n<=payloadLimit && n<=byteLimit-total,"FS preallocation bound");Bytes b(n);transfer(b.data(),n);return b;}
 void sendEvent(char tag,const std::string&label,const Bytes&b){send(eventHeader(tag,transcript->events(),label,b.size()));send(b);max_payload=std::max<std::uint64_t>(max_payload,b.size());}
 Bytes readEvent(char tag,const std::string&label){auto b=receive(17);Reader h{b};need(h.number(1)==std::uint8_t(tag) && h.number(8)==transcript->events(),"FS reordered/unknown event or sequence");auto n=h.number(8);h.end();need(n==label.size() && n>0 && n<=256,"FS unexpected label length");auto text=receive(n);need(text==Bytes(label.begin(),label.end()),"FS unexpected/reordered message label");b=receive(8);Reader length{b};auto size=length.number(8);length.end();need(size<=payloadLimit && size<=byteLimit-total,"FS message length bound");max_payload=std::max<std::uint64_t>(max_payload,size);return receive(size);}
};
FsIO::FsIO(bool p,const std::string&path,const Binding&b):impl_(std::make_unique<Impl>(p)){
 validate(b);auto&v=*impl_;Bytes h;
 if(p){OSRandom random;auto nonce=random.bytes(32);h=header(b,nonce);std::filesystem::path target(path);auto parent=target.parent_path();if(parent.empty())parent=".";v.final_name=target.filename().string();need(!v.final_name.empty() && v.final_name!="." && v.final_name!=".." && v.final_name.size()<=128,"FS output filename");v.dir=::open(parent.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);need(v.dir>=0,"FS output directory");struct stat st{};need(fstat(v.dir,&st)==0 && st.st_uid==getuid(),"FS output directory ownership");errno=0;need(fstatat(v.dir,v.final_name.c_str(),&st,AT_SYMLINK_NOFOLLOW)<0 && errno==ENOENT,"FS output exists; no overwrite");v.temp_name=v.final_name+".partial."+hex(r1::hash(nonce));v.fd=::openat(v.dir,v.temp_name.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);need(v.fd>=0,"FS exclusive temporary output");Writer start;start.raw(Bytes{'Z','K','R','7','F','S','1',0});start.number(h.size());v.send(start.b);v.send(h);
 }else{v.fd=::open(path.c_str(),O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);need(v.fd>=0 && fstat(v.fd,&v.original)==0 && S_ISREG(v.original.st_mode) && v.original.st_size>0 && std::uint64_t(v.original.st_size)<=byteLimit,"FS regular bounded proof input");auto magic=v.receive(12);Reader r{magic};need(r.raw(8)==Bytes({'Z','K','R','7','F','S','1',0}),"FS magic: interactive/foreign proof rejected");auto n=r.number();r.end();need(n>0 && n<=4096,"FS header length bound");h=v.receive(n);nonceFrom(h,b);}
 v.transcript=std::make_unique<CanonicalTranscript>(h,b.public_instance);Hash session;session.text("zkGPT.R7.FS.session.v1");session.add(bytes(v.transcript->prefix()));v.sid=hex(session.digest());
}
FsIO::~FsIO()=default;
bool FsIO::prover()const{return impl_->prover;}std::string FsIO::session()const{return impl_->sid;}Digest FsIO::prefix()const{return impl_->transcript->prefix();}
Bytes FsIO::message(const std::string&s,const Bytes&b){auto&v=*impl_;need(!v.finished,"FS closed session");Bytes out;if(v.prover){v.sendEvent('P',s,b);out=b;}else out=v.readEvent('P',s);v.transcript->message(s,out);return out;}
Fr FsIO::coin(const std::string&s){need(!impl_->finished,"FS closed session");return impl_->transcript->coin(s);}
void FsIO::finish(){auto&v=*impl_;need(!v.finished,"FS repeated completion");Writer w;w.number(v.transcript->events(),8);w.number(v.transcript->messages(),8);w.number(v.transcript->coins(),8);w.raw(bytes(prefix()));w.raw(bytes(v.transcript->challengeDigest()));
 if(v.prover){v.sendEvent('F',"end",w.b);auto t=Clock::now();need(::fsync(v.fd)==0,"FS proof fsync");need(::syscall(SYS_renameat2,v.dir,v.temp_name.c_str(),v.dir,v.final_name.c_str(),RENAME_NOREPLACE)==0,"FS atomic no-replace publication");need(::fsync(v.dir)==0,"FS directory fsync");v.io_seconds+=elapsed(t);
 }else{need(v.readEvent('F',"end")==w.b,"FS completion/challenge/transcript digest mismatch");std::uint8_t trailing=0;ssize_t n;do{n=::read(v.fd,&trailing,1);}while(n<0 && errno==EINTR);need(n==0 && v.total==std::uint64_t(v.original.st_size),"FS appended data rejected");struct stat now{};need(fstat(v.fd,&now)==0 && now.st_size==v.original.st_size && now.st_mtim.tv_sec==v.original.st_mtim.tv_sec && now.st_mtim.tv_nsec==v.original.st_mtim.tv_nsec && now.st_ctim.tv_sec==v.original.st_ctim.tv_sec && now.st_ctim.tv_nsec==v.original.st_ctim.tv_nsec,"FS concurrent proof modification rejected");}
 v.finished=true;
}
std::uint64_t FsIO::frames()const{return impl_->transcript->events();}std::uint64_t FsIO::wireBytes()const{return impl_->total;}std::uint64_t FsIO::proofBytes()const{return impl_->total;}
std::string FsIO::metadataJson()const{auto&v=*impl_;std::ostringstream o;o<<"{\"security_status\":\"FS_NIZK_CANDIDATE\",\"suite\":\""<<suite<<"\",\"session\":\""<<v.sid<<"\",\"complete\":"<<(v.finished?"true":"false")<<",\"proof_bytes\":"<<v.total<<",\"proof_sha256\":\""<<hex(v.file_hash.digest())<<"\",\"transcript_digest\":\""<<hex(prefix())<<"\",\"challenge_digest\":\""<<hex(v.transcript->challengeDigest())<<"\",\"challenges\":"<<v.transcript->coins()<<",\"prover_messages\":"<<v.transcript->messages()<<",\"events\":"<<frames()<<",\"field_rejections\":"<<v.transcript->rejections()<<",\"hash_seconds\":"<<v.transcript->seconds()+v.file_hash.seconds<<",\"file_io_seconds\":"<<v.io_seconds<<",\"max_message_payload_bytes\":"<<v.max_payload<<",\"production_gate\":\"BLOCKED_CRYPTOGRAPHIC_DESIGN\",\"may_enter_stage2\":false}";return o.str();}
}
