#pragma once
#include "lasso_table.hpp"
namespace route_b::lasso {
LassoProof prove(const LassoLookupInstance &instance, const std::vector<Fr> &query);
}
