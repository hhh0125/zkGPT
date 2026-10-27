#include "oneblock_bindings.hpp"
#include <openssl/evp.h>
#include <array>
#include <algorithm>
#include <set>

namespace zkgpt::r7::oneblock {
namespace {

// A split equality table uses O(sqrt(padded)) fields even for 4M-coordinate
// FC terminals. Indices and repeated coefficients retain little-endian order.
class Weights {
 std::vector<Fr>low_,high_;unsigned split_=0;std::size_t mask_=0;
 public:
 explicit Weights(const std::vector<Fr>&point){
  split_=unsigned(point.size()/2);low_=r1::weights({point.begin(),point.begin()+split_});
  high_=r1::weights({point.begin()+split_,point.end()});mask_=low_.size()-1;
 }
 Fr at(std::size_t j)const{return low_[j&mask_]*high_[j>>split_];}
};

// Hash every canonical (u64le index, scalar bias) pair in order, without
// retaining the map or constructing a multi-megabyte transport frame.
class MappingDigest {
 std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>context_{EVP_MD_CTX_new(),EVP_MD_CTX_free};
 std::array<unsigned char,32768>buffer_{};std::size_t used_=0;
 void flush(){if(used_){need(EVP_DigestUpdate(context_.get(),buffer_.data(),used_)==1,"Oneblock mapping digest update");used_=0;}}
 public:
 MappingDigest(){need(context_ && EVP_DigestInit_ex(context_.get(),EVP_sha256(),nullptr)==1,"Oneblock mapping digest initialization");}
 void append(std::size_t index,const Fr&bias){
  need(bias.isValid(),"Oneblock mapped coordinate bias scalar");if(used_+40>buffer_.size())flush();
  const auto coordinate=std::uint64_t(index);for(unsigned j=0;j<8;++j)buffer_[used_++]=std::uint8_t(coordinate>>(8*j));
  need(bias.serialize(buffer_.data()+used_,32)==32,"Oneblock mapped bias encoding");used_+=32;
 }
 Digest finish(){flush();Digest out{};unsigned size=0;need(EVP_DigestFinal_ex(context_.get(),out.data(),&size)==1 && size==out.size(),"Oneblock mapping digest finalization");return out;}
};

Fr biasAt(const MappedMasterClaim&claim,std::size_t j){return claim.coordinate_bias?claim.coordinate_bias(j):claim.bias;}

Fr foldPublic(std::vector<Fr>&kernel,const std::vector<Fr>&point){
 need(r1::log2exact(kernel.size())==point.size(),"Oneblock mapped public kernel dimension");std::size_t current=kernel.size();
 for(auto&r:point){for(std::size_t j=0;j<current/2;++j)kernel[j]=kernel[2*j]+r*(kernel[2*j+1]-kernel[2*j]);current/=2;}
 need(current==1,"Oneblock mapped public kernel terminal");return kernel[0];
}

}

void authenticateMappedClaims(IO&io,const masterB::Registered&registered,const master3::Secret*secret,
 const Value&masterValue,const std::vector<MappedMasterClaim>&claims,Random*rng,const std::string&role){
 const auto&master=registered.get(io);master3::validate(master.spec);
 need(!role.empty() && role.size()<=80 && !claims.empty() && claims.size()<=1024,"Oneblock mapped binding instance bound");
 need(io.prover()?(secret && masterValue && rng):(!secret && !masterValue && !rng),"Oneblock mapped binding P/V private boundary");
 if(io.prover())need(master3::encodeMaster(secret->public_pc)==master3::encodeMaster(master),"Oneblock mapped private Master identity");
 Writer statement;statement.text("R7-ONEBLOCK-MAPPED-MASTER-BATCH-v1");statement.text(master3::identity(master));
 statement.text(master.spec.layout);statement.text(io.session());statement.number(master.spec.actual,8);statement.number(master.spec.N,8);statement.number(claims.size());
 std::set<std::string>ids;std::vector<Digest>map_digests;std::vector<Fr>bias_evaluations;
 // All original handles, query points, mapping identities and bias values are
 // fixed and publicly bound before the first verifier aggregation challenge.
 for(const auto&claim:claims){
  need(!claim.id.empty() && claim.id.size()<=96 && ids.insert(claim.id).second,"Oneblock mapped claim identity");
  need(claim.index && claim.actual>0 && claim.actual<=claim.padded && claim.padded<=(1u<<24) && !(claim.padded&(claim.padded-1)),"Oneblock mapped claim source dimensions");
  need(claim.point.size()==r1::log2exact(claim.padded) && claim.bias.isValid(),"Oneblock mapped claim query dimensions");
  validate(claim.value.handle,io.session(),claim.value.handle.role);for(auto&r:claim.point)need(r.isValid(),"Oneblock mapped query scalar");
  Weights weights(claim.point);MappingDigest digest;Fr bias=0;
  for(std::size_t j=0;j<claim.actual;++j){auto index=claim.index(j);need(index<master.spec.actual,"Oneblock mapped index outside actual Master");auto b=biasAt(claim,j);digest.append(index,b);bias+=weights.at(j)*b;}
  map_digests.push_back(digest.finish());bias_evaluations.push_back(bias);
  statement.text(claim.id);statement.number(claim.actual,8);statement.number(claim.padded,8);statement.scalar(claim.bias);
  statement.number(claim.coordinate_bias?1:0);statement.raw(Bytes(map_digests.back().begin(),map_digests.back().end()));
  statement.raw(encode(claim.value.handle));for(auto&r:claim.point)statement.scalar(r);
 }
 bindPublic(io,role+"/claims",statement.b);
 std::vector<Fr>lambda;std::vector<Hidden>handles;Fr public_bias=0;
 for(std::size_t i=0;i<claims.size();++i){lambda.push_back(io.coin(role+"/lambda/"+std::to_string(i)));need(!lambda.back().isZero(),"Oneblock mapped zero aggregation challenge");handles.push_back(claims[i].value);public_bias+=lambda.back()*bias_evaluations[i];}
 auto initial=combine(io.session(),role+"/aggregate",handles,lambda);initial.value-=public_bias;initial.handle.point-=scalarCommit(public_bias,Fr(0));
 bindPublic(io,role+"/aggregate",encode(initial.handle));

 std::vector<Fr>kernel(master.spec.N,Fr(0));
 for(std::size_t i=0;i<claims.size();++i){
  const auto&claim=claims[i];Weights weights(claim.point);MappingDigest digest;
  for(std::size_t j=0;j<claim.actual;++j){auto index=claim.index(j);need(index<master.spec.actual,"Oneblock mapped index outside actual Master");auto b=biasAt(claim,j);digest.append(index,b);kernel[index]+=lambda[i]*weights.at(j);}
  // The trusted callbacks may borrow a graph, but cannot silently change that
  // graph between the public identity binding and reduction construction.
  need(digest.finish()==map_digests[i],"Oneblock mapped source changed after binding");
 }
 Expression relation;relation.factor_count=2;relation.terms.push_back({Fr(1),{0,1}});std::vector<Value>factors;
 if(io.prover()){
  factors.push_back([&](std::size_t j){return j<master.spec.actual?masterValue(j):Fr(0);});
  factors.push_back([&](std::size_t j){return kernel.at(j);});
 }
 auto reduction=expressionSumcheck(io,master.spec.N,initial,factors,relation,rng,role+"/reduction");
 need(reduction.endpoints.size()==2 && reduction.point.size()==r1::log2exact(master.spec.N),"Oneblock mapped reduction endpoints");
 auto public_kernel=foldPublic(kernel,reduction.point);
 equalHidden(io,reduction.endpoints[1],public_kernel,rng,role+"/public-kernel");
 // Reduction has consumed the kernel. Release its full-N storage before the
 // independent registered-Master A.3 folding and knowledge checks.
 std::vector<Fr>().swap(kernel);
 registered.opening(io,{reduction.point,reduction.endpoints[0].handle},secret,
                    io.prover()?&reduction.endpoints[0]:nullptr,rng,role+"/master-terminal");
}
}
