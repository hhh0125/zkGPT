#pragma once
#include "opt2_metrics.hpp"
#include <array>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
namespace zkgpt::r7::perf5 {
enum Region { Total, TreeAllocInit, TreeLeaf, TreeParent, TreeFree, Weights,
 ExpressionInit, Polynomial, Messages, Challenge, Fold, Endpoint, Equality,
 Carry, LeafAuthentication, Coverage, Count };
inline constexpr const char* names[]={"total","tree_alloc_init","tree_leaf_callback","tree_parent","tree_free","weights","expression_init","polynomial_callback_arithmetic_reduction","messages_and_recurrence_verification","challenge_and_derived","fold_callback_arithmetic","endpoint_authentication","equality_authentication","carry_and_binding","leaf_authentication","coverage"};
struct Stat {double wall=0,exclusive=0;std::uint64_t calls=0,allocs=0,bytes=0,exclusive_allocs=0,exclusive_bytes=0;};
struct Context {std::array<Stat,Count> stats{};std::uint64_t weight_calls=0,weight_elements=0,weight_muls=0,weight_writes=0;};
inline thread_local Context* context=nullptr;
class Scope {
 using Clock=std::chrono::steady_clock;
 inline static thread_local Scope* current=nullptr;
 Context* ctx_;Region region_;Scope* parent_=nullptr;Clock::time_point start_;
 double child_=0;std::uint64_t child_allocs_=0,child_bytes_=0;
 std::optional<crypto::AllocationTrace> trace_;
 public:explicit Scope(Region r):ctx_(context),region_(r){if(ctx_){trace_.emplace();parent_=current;current=this;start_=Clock::now();}}
 ~Scope(){if(!ctx_)return;auto wall=std::chrono::duration<double>(Clock::now()-start_).count();auto a=trace_->count(),b=trace_->bytes();auto&s=ctx_->stats[region_];++s.calls;s.wall+=wall;s.exclusive+=wall-child_;s.allocs+=a;s.bytes+=b;s.exclusive_allocs+=a-child_allocs_;s.exclusive_bytes+=b-child_bytes_;current=parent_;if(parent_){parent_->child_+=wall;parent_->child_allocs_+=a;parent_->child_bytes_+=b;}}
};
class Product {
 std::optional<Context> data_;Context* previous_;const std::string& role_;std::size_t n_;bool prover_,enabled_;
 public:Product(const std::string&role,std::size_t n,bool p):previous_(context),role_(role),n_(n),prover_(p),enabled_(std::getenv("ZKGPT_PERF5_PROFILE")!=nullptr){if(enabled_){data_.emplace();context=&*data_;}}
 ~Product(){if(!enabled_)return;context=previous_;std::cerr<<std::setprecision(12)<<"PERF5_PRODUCT {\"role\":"<<std::quoted(role_)<<",\"n\":"<<n_<<",\"prover\":"<<(prover_?"true":"false")<<",\"regions\":{";for(unsigned i=0;i<Count;++i){if(i)std::cerr<<',';auto&s=data_->stats[i];std::cerr<<std::quoted(names[i])<<":{\"calls\":"<<s.calls<<",\"inclusive_wall\":"<<s.wall<<",\"exclusive_wall\":"<<s.exclusive<<",\"allocations\":"<<s.allocs<<",\"allocated_bytes\":"<<s.bytes<<",\"exclusive_allocations\":"<<s.exclusive_allocs<<",\"exclusive_allocated_bytes\":"<<s.exclusive_bytes<<'}';}std::cerr<<"},\"weights\":{\"calls\":"<<data_->weight_calls<<",\"elements\":"<<data_->weight_elements<<",\"field_multiplications\":"<<data_->weight_muls<<",\"analytical_written_bytes\":"<<data_->weight_writes<<"}}\n";}
};
inline void weights(std::size_t n){if(context){++context->weight_calls;context->weight_elements+=n;context->weight_muls+=2*(n-1);context->weight_writes+=(2*(n-1)+n)*32;}}
}
