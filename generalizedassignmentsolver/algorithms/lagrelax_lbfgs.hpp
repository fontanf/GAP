#pragma once

#if DLIB_FOUND

#include "generalizedassignmentsolver/solution.hpp"

namespace generalizedassignmentsolver
{

/************************** lagrelax_assignment_lbfgs *************************/

struct LagRelaxAssignmentLbfgsOptionalParameters
{
    Info info = Info();

    std::vector<int>* initial_multipliers = NULL;
    std::vector<int>* fixed_alt = NULL; // -1: unfixed, 0: fixed to 0, 1: fixed to 1.
};

struct LagRelaxAssignmentLbfgsOutput: Output
{
    LagRelaxAssignmentLbfgsOutput(const Instance& instance, Info& info): Output(instance, info) { }
    LagRelaxAssignmentLbfgsOutput& algorithm_end(Info& info);

    std::vector<double> x; // vector of size instance.alternative_number()
    std::vector<double> multipliers; // vector of size instance.item_number()
};

LagRelaxAssignmentLbfgsOutput lagrelax_assignment_lbfgs(const Instance& instance, LagRelaxAssignmentLbfgsOptionalParameters p = {});

/*************************** lagrelax_knapsack_lbfgs **************************/

struct LagRelaxKnapsackLbfgsOutput: Output
{
    LagRelaxKnapsackLbfgsOutput(const Instance& instance, Info& info): Output(instance, info) { }
    LagRelaxKnapsackLbfgsOutput& algorithm_end(Info& info);

    std::vector<double> x; // vector of size instance.alternative_number()
    std::vector<double> multipliers; // vector of size instance.agent_number()
};

LagRelaxKnapsackLbfgsOutput lagrelax_knapsack_lbfgs(const Instance& instance, Info info = Info());

}

#endif

