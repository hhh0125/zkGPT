#pragma once
#include "paper_r7_unified_input.hpp"
#include "bounded_workers.hpp"

namespace zkgpt::r7::oneblock::unified::detail {
class Workers : public zkgpt::Workers { public: using zkgpt::Workers::Workers; };
}
