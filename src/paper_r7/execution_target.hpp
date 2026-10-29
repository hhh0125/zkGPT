#pragma once
#include "framework.hpp"
namespace zkgpt::r7 {
struct ExecutionTarget {
 unsigned blocks;
 const char* manifest_path;
 const char* manifest_pin;
 const char* circuit;
};
inline ExecutionTarget executionTarget(unsigned blocks){
 r1::require(blocks==1 || blocks==12,"R7 supports only native R6 oneblock or Full12");
 if(blocks==12)return {12,"reports/R6A/full12-model-manifest-v1.txt","b9a948a34b7f68c2efc0c6a36a3353f918380c0556e259e9f9c0b3d7604ba5c1","ac9d0926a13049f10e1a252642508519c13ecef34aa39e42d1813e1cd30b22c7"};
 return {1,"reports/R6A/oneblock-model-manifest-v1.txt","3ab43263570f08a2fe314bca99f2f95c11020abae17eca587f1717f52d298073","1cf3bc83a9a9f289e540238255d467b7fa6e2801209aa00f07fe232c48635a9a"};
}
}
