#pragma once
#include "paper_r7_oneblock_sumcheck.hpp"
#include <array>
#include <chrono>
namespace zkgpt::r7::oneblock::native_masked {
inline constexpr const char* suite="R7-NATIVE-GKR-MASKED-QUADRATIC-HIDDEN-A3-RESEARCH-v1";
enum class Mode { Sigma, MaskedResearch };
Mode configuredMode();
inline thread_local Mode active=Mode::Sigma;
class ScopedMode {
 Mode previous_;
 public:explicit ScopedMode(Mode mode):previous_(active){active=mode;}
 ~ScopedMode(){active=previous_;}
 ScopedMode(const ScopedMode&)=delete;ScopedMode&operator=(const ScopedMode&)=delete;
};
inline bool enabled(){return active==Mode::MaskedResearch;}
struct PhaseMeasure {
 std::string role;bool prover,masked;std::size_t rounds=0;std::array<double,6>seconds{};
 std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
 PhaseMeasure(std::string name,bool isP,bool mask):role(std::move(name)),prover(isP),masked(mask){}
 PhaseMeasure(const PhaseMeasure&)=delete;PhaseMeasure&operator=(const PhaseMeasure&)=delete;
 ~PhaseMeasure();
};
class Mask {
 unsigned variables_;std::size_t padded_=1;std::vector<Fr>coefficients_,point_;Fr blind_=0,rho_=0,claim_=0;
 Fr evaluated_prefix_=0,remaining_sum_=0;
 G1 commitment_;std::string role_;PhaseMeasure&measure_;bool finished_=false;
 void open(IO&,const Hidden&,const std::vector<Fr>&,Random*,const std::string&);
 public:
 Mask(IO&,unsigned,const Hidden&,Random*,std::string,PhaseMeasure&);
 Fr round(IO&,const std::array<Fr,3>&);
 Hidden finish(IO&,Random*);
};
Reduction matrix(IO&,std::size_t,const Hidden&,const std::vector<Value>&,Random*,const std::string&,detail::MatrixArithmetic* =nullptr);
}
