#pragma once
#include "paper_r7_opt2_metrics.hpp"
#include "paper_r7_perf6_options.hpp"
#include <array>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
namespace zkgpt::r7::perf5 {
enum Region { Total, TreeAllocInit, TreeLeaf, TreeParent, TreeFree, Weights,
 ExpressionInit, Polynomial, Messages, Challenge, Fold, Endpoint, Equality,
 Carry, LeafAuthentication, Coverage, LeafWeights, Recurrence, InputsConstruct, BatchAggregation, BatchSigma, A2Polynomial, C2Input, C2Fold, B2Message, IndependentProduct, BatchedProduct, PolynomialReduction, PolynomialFirst, PolynomialLater, FoldFirst, FoldLater, LeafDot, FingerprintRoot, PairedLeafParent, MaskGeneration, MaskCommitment, MaskSumOpening, MaskEndpointOpening, A3, Transcript, Count };
inline constexpr const char* names[]={"total","tree_alloc_init","tree_leaf_callback","tree_parent","tree_free","weights","expression_init","polynomial_callback_arithmetic_reduction","messages_and_recurrence_verification","challenge_and_derived","fold_callback_arithmetic","endpoint_authentication","equality_authentication","carry_and_binding","leaf_authentication","coverage","leaf_evaluate_weights","recurrence_verification","product_inputs_construct","batch_polynomial_aggregation","batch_sigma","A2_product_polynomial_seconds","C2_contiguous_input_seconds","C2_fold_seconds","B2_message_seconds","independent_product_seconds","batched_product_seconds","polynomial_reduction","polynomial_first","polynomial_later","fold_first","fold_later","leaf_dot","fingerprint_and_root","paired_leaf_and_first_parent","mask_generation","mask_commitment","mask_sum_opening","mask_endpoint_opening","a3","transcript"};
struct Stat {double wall=0,exclusive=0;std::uint64_t calls=0,allocs=0,bytes=0,exclusive_allocs=0,exclusive_bytes=0;};
enum class Kernel { SinglePolynomial, BatchPolynomial, Tree, FirstFold, LaterFold, LeafDot, Weights,
                    Fingerprint, PairedLeafParent, A3GeneratorFold, Count };
inline constexpr const char* kernel_names[]={"single_polynomial","batched_polynomial","tree_parent","first_fold","later_fold","leaf_dot","equality_weights","fingerprint_root","paired_leaf_parent","a3_generator_fold"};
struct WorkerStat {
 std::uint64_t calls=0,units=0;
 double wall=0,busy=0,dispatch_and_wait=0;
};
struct Context {unsigned poly_mode=0,poly_workers=0,tree_workers=0;std::uint64_t parallel_poly_rounds=0,parallel_fold_jobs=0,parallel_tree_layers=0,poly_callbacks=0,fold_callbacks=0,independent_calls=0,batched_calls=0;std::uint64_t poly_mul=0,poly_add=0,poly_pairs=0,poly_rounds=0,fold_mul=0,fold_add=0;std::array<Stat,Count> stats{};bool workers_enabled=false;std::array<std::array<WorkerStat,32>,unsigned(Kernel::Count)> workers{};perf6::Options options={true,true,perf6::enabled("ZKGPT_PERF6_B2")};
 std::uint64_t a2_calls=0,a2_pairs=0,generic_product_calls=0,c2_polynomial_calls=0,c2_polynomial_pairs=0,c2_fold_calls=0,c2_fold_pairs=0,input_constructions=0,prepared_leaf_reuses=0;
std::uint64_t weight_calls=0,weight_elements=0,weight_muls=0,weight_writes=0;
 std::uint64_t leaf_weight_calls=0,leaf_weight_elements=0,leaf_weight_muls=0,leaf_weight_payload_bytes=0;};
inline thread_local Context* context=nullptr;
class WorkerDispatch {
 using Clock=std::chrono::steady_clock;
 struct alignas(64) Sample {double wall;};
 Context* ctx_;
 Kernel kernel_;
 std::size_t units_;
 unsigned partitions_;
 Clock::time_point start_;
 std::array<Sample,32> work_;
public:
 WorkerDispatch(Kernel kernel,std::size_t units,unsigned partitions)
  :ctx_(context && context->workers_enabled?context:nullptr),
   kernel_(kernel),units_(units),partitions_(partitions){
  if(ctx_){start_=Clock::now();for(unsigned i=0;i<partitions_;++i)work_[i].wall=0;}
 }
 template<class Work> void participant(unsigned i,const Work& work){
  if(!ctx_){work();return;}
  const auto start=Clock::now();
   work();
  work_[i].wall=std::chrono::duration<double>(Clock::now()-start).count();
 }
 ~WorkerDispatch(){
  if(!ctx_)return;
  auto& totals=ctx_->workers[unsigned(kernel_)][partitions_-1];
  const auto wall=std::chrono::duration<double>(Clock::now()-start_).count();
  ++totals.calls;totals.units+=units_;totals.wall+=wall;
  for(unsigned i=0;i<partitions_;++i)totals.busy+=work_[i].wall;
  totals.dispatch_and_wait+=wall-work_[0].wall;
 }
};
class Scope {
 using Clock=std::chrono::steady_clock;
 inline static thread_local Scope* current=nullptr;
 Context* ctx_;Region region_;Scope* parent_=nullptr;Clock::time_point start_;
 double child_=0;std::uint64_t child_allocs_=0,child_bytes_=0;
 std::optional<crypto::AllocationTrace> trace_;
 public:explicit Scope(Region r,bool active=true):ctx_(active?context:nullptr),region_(r){if(ctx_){trace_.emplace();parent_=current;current=this;start_=Clock::now();}}
 ~Scope(){if(!ctx_)return;auto wall=std::chrono::duration<double>(Clock::now()-start_).count();auto a=trace_->count(),b=trace_->bytes();auto&s=ctx_->stats[region_];++s.calls;s.wall+=wall;s.exclusive+=wall-child_;s.allocs+=a;s.bytes+=b;s.exclusive_allocs+=a-child_allocs_;s.exclusive_bytes+=b-child_bytes_;current=parent_;if(parent_){parent_->child_+=wall;parent_->child_allocs_+=a;parent_->child_bytes_+=b;}}
};
class Product {
 std::optional<Context> data_;Context* previous_;const std::string& role_;std::size_t n_;bool prover_,enabled_;
 public:Product(const std::string&role,std::size_t n,bool p):previous_(context),role_(role),n_(n),prover_(p),enabled_(std::getenv("ZKGPT_PERF5_PROFILE")!=nullptr){if(enabled_){data_.emplace();data_->workers_enabled=std::getenv("ZKGPT_PERF5_WORKER_PROFILE")!=nullptr;context=&*data_;}}
 ~Product(){if(!enabled_)return;
  if(data_->workers_enabled){
   for(unsigned kernel=0;kernel<unsigned(Kernel::Count);++kernel){
    for(unsigned workers=0;workers<32;++workers){
     const auto& stat=data_->workers[kernel][workers];if(!stat.calls)continue;
     std::cerr<<std::setprecision(12)<<"PRODUCT_WORKERS {\"role\":"<<std::quoted(role_)<<",\"n\":"<<n_
      <<",\"prover\":"<<(prover_?"true":"false")<<",\"kernel\":"<<std::quoted(kernel_names[kernel])
      <<",\"actual_workers\":"<<workers+1<<",\"calls\":"<<stat.calls<<",\"work_units\":"<<stat.units
      <<",\"wall\":"<<stat.wall<<",\"worker_busy_sum\":"<<stat.busy<<",\"controller_dispatch_and_wait\":"
      <<stat.dispatch_and_wait<<"}\n";
    }
   }
  }
std::cerr<<"POLY_ARITHMETIC {\"role\":"<<std::quoted(role_)<<",\"n\":"<<n_<<",\"prover\":"<<(prover_?"true":"false")<<",\"mode\":"<<data_->poly_mode<<",\"pairs\":"<<data_->poly_pairs<<",\"rounds\":"<<data_->poly_rounds<<",\"polynomial_workers\":"<<data_->poly_workers<<",\"parallel_polynomial_rounds\":"<<data_->parallel_poly_rounds<<",\"parallel_fold_jobs\":"<<data_->parallel_fold_jobs<<",\"tree_workers\":"<<data_->tree_workers<<",\"parallel_tree_layers\":"<<data_->parallel_tree_layers<<",\"polynomial_callbacks\":"<<data_->poly_callbacks<<",\"fold_callbacks\":"<<data_->fold_callbacks<<",\"independent_calls\":"<<data_->independent_calls<<",\"batched_calls\":"<<data_->batched_calls<<",\"polynomial_mul\":"<<data_->poly_mul<<",\"polynomial_add_sub\":"<<data_->poly_add<<",\"fold_mul\":"<<data_->fold_mul<<",\"fold_add_sub\":"<<data_->fold_add<<",\"counts_kind\":\"executed_dimensions_formula_validated_by_counted_kernel\"}\n";context=previous_;std::cerr<<std::setprecision(12)<<"PERF5_PRODUCT {\"role\":"<<std::quoted(role_)<<",\"n\":"<<n_<<",\"prover\":"<<(prover_?"true":"false")<<",\"regions\":{";for(unsigned i=0;i<Count;++i){if(i)std::cerr<<',';auto&s=data_->stats[i];std::cerr<<std::quoted(names[i])<<":{\"calls\":"<<s.calls<<",\"inclusive_wall\":"<<s.wall<<",\"exclusive_wall\":"<<s.exclusive<<",\"allocations\":"<<s.allocs<<",\"allocated_bytes\":"<<s.bytes<<",\"exclusive_allocations\":"<<s.exclusive_allocs<<",\"exclusive_allocated_bytes\":"<<s.exclusive_bytes<<'}';}std::cerr<<"},\"optimization\":{\"A2_enabled\":"<<(data_->options.a2?"true":"false")<<",\"C2_enabled\":"<<(data_->options.c2?"true":"false")<<",\"B2_enabled\":"<<(data_->options.b2?"true":"false")<<",\"a2_calls\":"<<data_->a2_calls<<",\"a2_pairs\":"<<data_->a2_pairs<<",\"a2_coefficient_multiplications_eliminated\":"<<4*data_->a2_pairs<<",\"generic_product_calls\":"<<data_->generic_product_calls<<",\"c2_polynomial_calls\":"<<data_->c2_polynomial_calls<<",\"c2_polynomial_pairs\":"<<data_->c2_polynomial_pairs<<",\"c2_fold_calls\":"<<data_->c2_fold_calls<<",\"c2_fold_pairs\":"<<data_->c2_fold_pairs<<",\"input_constructions\":"<<data_->input_constructions<<",\"input_copy_bytes\":0,\"prepared_leaf_reuses\":"<<data_->prepared_leaf_reuses<<"},\"weights\":{\"calls\":"<<data_->weight_calls<<",\"elements\":"<<data_->weight_elements<<",\"field_multiplications\":"<<data_->weight_muls<<",\"analytical_written_bytes\":"<<data_->weight_writes<<",\"vector_payload_bytes\":"<<data_->weight_elements*sizeof(crypto::Fr)<<"},\"leaf_evaluate_weights\":{\"calls\":"<<data_->leaf_weight_calls<<",\"elements\":"<<data_->leaf_weight_elements<<",\"field_multiplications\":"<<data_->leaf_weight_muls<<",\"vector_payload_bytes\":"<<data_->leaf_weight_payload_bytes<<"}}\n";}
};
inline void weights(std::size_t n){if(context){++context->weight_calls;context->weight_elements+=n;context->weight_muls+=2*(n-1);context->weight_writes+=(2*(n-1)+n)*32;}}
inline void leafWeights(std::size_t n){if(context){++context->leaf_weight_calls;context->leaf_weight_elements+=n;context->leaf_weight_muls+=n-1;context->leaf_weight_payload_bytes+=n*sizeof(crypto::Fr);}}
}
