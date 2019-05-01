#pragma once

#include "gap/lib/instance.hpp"
#include "gap/lib/solution.hpp"

namespace gap
{

Solution sol_vdns_simple(const Instance& ins, Solution& sol, std::default_random_engine& gen, Info info = Info());

}
