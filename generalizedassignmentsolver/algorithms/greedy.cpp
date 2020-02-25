#include "generalizedassignmentsolver/algorithms/greedy.hpp"

#include <set>
#include <random>
#include <algorithm>
#include <vector>

using namespace generalizedassignmentsolver;

std::vector<std::pair<ItemIdx, AgentIdx>> generalizedassignmentsolver::greedy_init(const Solution& sol, const Desirability& f)
{
    const Instance& ins = sol.instance();
    ItemIdx n = ins.item_number();
    AgentIdx m = ins.agent_number();
    std::vector<std::pair<ItemIdx, AgentIdx>> alt(ins.alternative_number());
    for (ItemIdx j=0; j<n; ++j) {
        for (AgentIdx i=0; i<m; ++i) {
            AltIdx k = ins.alternative_index(j, i);
            alt[k] = {j, i};
        }
    }
    sort(alt.begin(), alt.end(), [&f](
                const std::pair<ItemIdx, AgentIdx>& a,
                const std::pair<ItemIdx, AgentIdx>& b) -> bool {
            return f(a.first, a.second) < f(b.first, b.second); });
    return alt;
}

void generalizedassignmentsolver::greedy(Solution& sol, const std::vector<std::pair<ItemIdx, AgentIdx>>& alt)
{
    const Instance& ins = sol.instance();
    for (auto p: alt) {
        ItemIdx j = p.first;
        if (sol.agent(j) != -1)
            continue;
        AgentIdx i = p.second;
        const Alternative& a = ins.alternative(j, i);
        if (sol.remaining_capacity(i) < a.w)
            continue;
        sol.set(j, i);
        if (sol.full())
            break;
    }
}

Output generalizedassignmentsolver::greedy(const Instance& ins, const Desirability& f, Info info)
{
    VER(info, "*** greedy " << f.to_string() << " ***" << std::endl);
    Output output(ins, info);
    Solution sol(ins);
    auto alt = greedy_init(sol, f);
    greedy(sol, alt);
    output.update_solution(sol, std::stringstream(""), info);
    return output.algorithm_end(info);
}

/******************************************************************************/

std::vector<std::vector<AgentIdx>> generalizedassignmentsolver::greedyregret_init(const Instance& ins, const Desirability& f)
{
    ItemIdx n = ins.item_number();
    AgentIdx m = ins.agent_number();

    std::vector<std::vector<AgentIdx>> agents(n, std::vector<AgentIdx>(m));
    for (ItemIdx j=0; j<n; ++j) {
        std::iota(agents[j].begin(), agents[j].end(), 0);
        sort(agents[j].begin(), agents[j].end(), [&f, &j](
                    AgentIdx i1, AgentIdx i2) -> bool {
                return f(j, i1) < f(j, i2); });
    }

    return agents;
}

void generalizedassignmentsolver::greedyregret(Solution& sol, const Desirability& f,
        const std::vector<std::vector<AgentIdx>>& agents,
        const std::vector<int>& fixed_alt)
{
    const Instance& ins = sol.instance();
    ItemIdx n = ins.item_number();
    AgentIdx m = ins.agent_number();
    std::vector<std::pair<AgentIdx, AgentIdx>> bests(n, {0, 1});

    while (!sol.full()) {
        ItemIdx j_best = -1;
        double f_best = -1;
        for (ItemIdx j=0; j<n; ++j) {
            if (sol.agent(j) != -1)
                continue;

            AgentIdx& i_first = bests[j].first;
            AgentIdx& i_second = bests[j].second;

            while (i_first < m) {
                AgentIdx i = agents[j][i_first];
                if (ins.alternative(j, i).w > sol.remaining_capacity(i) ||
                        (!fixed_alt.empty() && fixed_alt[ins.alternative_index(j, i)] != -1)) {
                    i_first++;
                    if (i_first == i_second)
                        i_second++;
                } else {
                    break;
                }
            }
            if (i_first == m)
                return;

            while (i_second < m) {
                AgentIdx i = agents[j][i_second];
                if (ins.alternative(j, i).w > sol.remaining_capacity(i) ||
                        (!fixed_alt.empty() && fixed_alt[ins.alternative_index(j, i)] != -1)) {
                    i_second++;
                } else {
                    break;
                }
            }

            double f_curr = (i_second == m)?
                std::numeric_limits<double>::infinity():
                f(j, agents[j][i_second]) - f(j, agents[j][i_first]);
            if (j_best == -1 || f_best < f_curr) {
                f_best = f_curr;
                j_best = j;
            }
        }
        sol.set(j_best, agents[j_best][bests[j_best].first]);
    }
}

Output generalizedassignmentsolver::greedyregret(const Instance& ins, const Desirability& f, Info info)
{
    VER(info, "*** greedyregret " << f.to_string() << " ***" << std::endl);
    Output output(ins, info);
    Solution sol(ins);
    auto agents = greedyregret_init(ins, f);
    greedyregret(sol, f, agents, {});
    output.update_solution(sol, std::stringstream(""), info);
    return output.algorithm_end(info);
}

/******************************************************************************/

void nshift(Solution& sol)
{
    const Instance& ins = sol.instance();
    ItemIdx n = ins.item_number();
    AgentIdx m = ins.agent_number();
    for (ItemIdx j=0; j<n; ++j) {
        AgentIdx i_old = sol.agent(j);
        const Alternative& a_old = ins.alternative(j, i_old);
        Cost c_best = 0;
        AgentIdx i_best = -1;
        for (AgentIdx i=0; i<m; ++i) {
            if (i == i_old)
                continue;
            const Alternative& a = ins.alternative(j, i);
            if (sol.remaining_capacity(i) >= a.w && c_best > a.c - a_old.c) {
                i_best = i;
                c_best = a.c - a_old.c;
            }
        }
        if (i_best != -1)
            sol.set(j, i_best);
    }
}

void generalizedassignmentsolver::mthg(Solution& sol, const std::vector<std::pair<ItemIdx, AgentIdx>>& alt)
{
    greedy(sol, alt);
    if (sol.feasible())
        nshift(sol);
}

Output generalizedassignmentsolver::mthg(const Instance& ins, const Desirability& f, Info info)
{
    VER(info, "*** mthg " << f.to_string() << " ***" << std::endl);
    Output output(ins, info);
    Solution sol(ins);
    auto alt = greedy_init(sol, f);
    mthg(sol, alt);
    output.update_solution(sol, std::stringstream(""), info);
    return output.algorithm_end(info);
}

void generalizedassignmentsolver::mthgregret(Solution& sol, const Desirability& f,
        const std::vector<std::vector<AgentIdx>>& agents,
        const std::vector<int>& fixed_alt)
{
    greedyregret(sol, f, agents, fixed_alt);
    if (sol.feasible())
        nshift(sol);
}

Output generalizedassignmentsolver::mthgregret(const Instance& ins, const Desirability& f, Info info)
{
    VER(info, "*** mthgregret " << f.to_string() << " ***" << std::endl);
    Output output(ins, info);
    Solution sol(ins);
    auto agents = greedyregret_init(ins, f);
    mthgregret(sol, f, agents, {});
    output.update_solution(sol, std::stringstream(""), info);
    return output.algorithm_end(info);
}
