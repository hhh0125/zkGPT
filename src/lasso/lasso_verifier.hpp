#pragma once
#include "lasso_types.hpp"
namespace route_b::lasso {
bool verify(const LassoLookupInstance &instance, const LassoProof &proof,
            VerifiedLookupClaim *claim = nullptr, std::string *error = nullptr);
}
