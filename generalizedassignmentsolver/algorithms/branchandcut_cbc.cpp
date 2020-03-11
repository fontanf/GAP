#if COINOR_FOUND

#include "generalizedassignmentsolver/algorithms/branchandcut_cbc.hpp"

#include "coin/CbcHeuristicDiveCoefficient.hpp"
#include "coin/CbcHeuristicDiveFractional.hpp"
#include "coin/CbcHeuristicDiveGuided.hpp"
#include "coin/CbcHeuristicDiveVectorLength.hpp"
#include "coin/CbcLinked.hpp"
#include "coin/CbcHeuristicGreedy.hpp"
#include "coin/CbcHeuristicFPump.hpp"
#include "coin/CbcHeuristicLocal.hpp"
#include "coin/CbcHeuristic.hpp"
#include "coin/CbcHeuristicRINS.hpp"
#include "coin/CbcHeuristicRENS.hpp"

#include "coin/CglAllDifferent.hpp"
#include "coin/CglClique.hpp"
#include "coin/CglDuplicateRow.hpp"
#include "coin/CglFlowCover.hpp"
#include "coin/CglGomory.hpp"
#include "coin/CglKnapsackCover.hpp"
#include "coin/CglLandP.hpp"
#include "coin/CglLiftAndProject.hpp"
#include "coin/CglMixedIntegerRounding.hpp"
#include "coin/CglMixedIntegerRounding2.hpp"
#include "coin/CglOddHole.hpp"
#include "coin/CglProbing.hpp"
#include "coin/CglRedSplit.hpp"
#include "coin/CglResidualCapacity.hpp"
#include "coin/CglSimpleRounding.hpp"
#include "coin/CglStored.hpp"
#include "coin/CglTwomir.hpp"

/**
 * Useful links:
 * https://github.com/coin-or/Cgl/wiki
 * https://github.com/coin-or/Cbc/blob/master/Cbc/examples/sample2.cpp
 * https://github.com/coin-or/Cbc/blob/master/Cbc/examples/sample3.cpp
 * Callback https://github.com/coin-or/Cbc/blob/master/Cbc/examples/inc.cpp
 */

using namespace generalizedassignmentsolver;

BranchAndCutCbcOutput& BranchAndCutCbcOutput::algorithm_end(Info& info)
{
    //PUT(info, "Algorithm", "Iterations", it);
    Output::algorithm_end(info);
    //VER(info, "Iterations: " << it << std::endl);
    return *this;
}

/********************************** Callback **********************************/

class SolHandler: public CbcEventHandler
{

public:

    virtual CbcAction event(CbcEvent whichEvent);
    SolHandler(const Instance& instance, BranchAndCutCbcOptionalParameters& p, BranchAndCutCbcOutput& output):
        CbcEventHandler(), instance_(instance), p_(p), output_(output) { }
    SolHandler(CbcModel *model, const Instance& instance, BranchAndCutCbcOptionalParameters& p, BranchAndCutCbcOutput& output):
        CbcEventHandler(model), instance_(instance), p_(p), output_(output) { }
    virtual ~SolHandler() { }
    SolHandler(const SolHandler &rhs): CbcEventHandler(rhs), instance_(rhs.instance_), p_(rhs.p_), output_(rhs.output_) { }
    SolHandler &operator=(const SolHandler &rhs)
    {
        if (this != &rhs) {
            CbcEventHandler::operator=(rhs);
            //this->instance_    = rhs.instance_;
            this->p_      = rhs.p_;
            this->output_ = rhs.output_;
        }
        return *this;
    }
    virtual CbcEventHandler *clone() const { return new SolHandler(*this); }

private:

    const Instance& instance_;
    BranchAndCutCbcOptionalParameters& p_;
    BranchAndCutCbcOutput& output_;

};

CbcEventHandler::CbcAction SolHandler::event(CbcEvent whichEvent)
{
    if ((model_->specialOptions() & 2048) != 0) // not in subtree
        return noAction;

    Cost lb = std::ceil(model_->getBestPossibleObjValue() - TOL);
    output_.update_lower_bound(lb, std::stringstream(""), p_.info);

    if ((whichEvent != solution && whichEvent != heuristicSolution)) // no solution found
        return noAction;

    OsiSolverInterface *origSolver = model_->solver();
    const OsiSolverInterface *pps = model_->postProcessedSolver(1);
    const OsiSolverInterface *solver = pps? pps: origSolver;

    if (!output_.solution.feasible() || output_.solution.cost() > solver->getObjValue() + 0.5) {
        const double *solution = solver->getColSolution();
        Solution sol_curr(instance_);
        for (AltIdx k = 0; k < instance_.alternative_number(); ++k)
            if (solution[k] > 0.5)
                sol_curr.set(k);
        output_.update_solution(sol_curr, std::stringstream(""), p_.info);
    }

    return noAction;
}

/******************************************************************************/

CoinLP::CoinLP(const Instance& instance)
{
    ItemIdx  n = instance.item_number();
    AgentIdx m = instance.agent_number();

    // Variables
    int col_number = instance.alternative_number();
    col_lower.resize(col_number, 0);
    col_upper.resize(col_number, 1);

    // Objective
    objective = std::vector<double>(col_number);
    for (AltIdx k = 0; k < col_number; ++k)
        objective[k] = instance.alternative(k).c;

    // Constraints
    int row_number = 0; // will be increased each time we add a constraint
    std::vector<CoinBigIndex> row_starts;
    std::vector<int> number_of_elements_in_rows;
    std::vector<int> element_columns;
    std::vector<double> elements;

    // Every item needs to be assigned
    // sum_i xij = 1 for all j
    for (ItemIdx j = 0; j < n; ++j) {
        // Initialize new row
        row_starts.push_back(elements.size());
        number_of_elements_in_rows.push_back(0);
        row_number++;
        // Add elements
        for (AgentIdx i = 0; i < m; ++i) {
            elements.push_back(1);
            element_columns.push_back(instance.alternative_index(j, i));
            number_of_elements_in_rows.back()++;
        }
        // Add row bounds
        row_lower.push_back(1);
        row_upper.push_back(1);
    }

    // Capacity constraint
    // sum_j wj xij <= ci
    for (AgentIdx i = 0; i < m; ++i) {
        // Initialize new row
        row_starts.push_back(elements.size());
        number_of_elements_in_rows.push_back(0);
        row_number++;
        // Add row elements
        for (ItemIdx j = 0; j < n; ++j) {
            elements.push_back(instance.alternative(j, i).w);
            element_columns.push_back(instance.alternative_index(j, i));
            number_of_elements_in_rows.back()++;
        }
        // Add row bounds
        row_lower.push_back(0);
        row_upper.push_back(instance.capacity(i));
    }

    // Create matrix
    row_starts.push_back(elements.size());
    matrix = CoinPackedMatrix(false,
            col_number, row_number, elements.size(),
            elements.data(), element_columns.data(),
            row_starts.data(), number_of_elements_in_rows.data());
}

BranchAndCutCbcOutput generalizedassignmentsolver::branchandcut_cbc(const Instance& instance, BranchAndCutCbcOptionalParameters p)
{
    VER(p.info, "*** branchandcut_cbc ***" << std::endl);

    BranchAndCutCbcOutput output(instance, p.info);

    ItemIdx n = instance.item_number();
    if (n == 0)
        return output.algorithm_end(p.info);

    CoinLP mat(instance);

    OsiCbcSolverInterface solver1;

    // Reduce printout
    solver1.getModelPtr()->setLogLevel(0);
    solver1.messageHandler()->setLogLevel(0);

    // Load problem
    solver1.loadProblem(mat.matrix, mat.col_lower.data(), mat.col_upper.data(),
              mat.objective.data(), mat.row_lower.data(), mat.row_upper.data());

    // Mark integer
    for (AltIdx k = 0; k < instance.alternative_number(); ++k)
        solver1.setInteger(k);

    // Pass data and solver to CbcModel
    CbcModel model(solver1);

    // Callback
    SolHandler sh(instance, p, output);
    model.passInEventHandler(&sh);

    // Reduce printout
    model.setLogLevel(0);
    model.solver()->setHintParam(OsiDoReducePrint, true, OsiHintTry);

    // Heuristics
    //CbcHeuristicDiveCoefficient heuristic_divecoefficient(model);
    //model.addHeuristic(&heuristic_divecoefficient);
    //CbcHeuristicDiveFractional heuristic_divefractional(model);
    //model.addHeuristic(&heuristic_divefractional);
    //CbcHeuristicDiveGuided heuristic_diveguided(model);
    //model.addHeuristic(&heuristic_diveguided);
    //CbcHeuristicDiveVectorLength heuristic_divevetorlength(model);
    //model.addHeuristic(&heuristic_divevetorlength);
    //CbcHeuristicDynamic3 heuristic_dynamic3(model); // crash
    //model.addHeuristic(&heuristic_dynamic3);
    //CbcHeuristicFPump heuristic_fpump(model);
    //model.addHeuristic(&heuristic_fpump);
    //CbcHeuristicGreedyCover heuristic_greedycover(model);
    //model.addHeuristic(&heuristic_greedycover);
    //CbcHeuristicGreedyEquality heuristic_greedyequality(model);
    //model.addHeuristic(&heuristic_greedyequality);
    //CbcHeuristicLocal heuristic_local(model);
    //model.addHeuristic(&heuristic_local);
    //CbcHeuristicPartial heuristic_partial(model);
    //model.addHeuristic(&heuristic_partial);
    //CbcHeuristicRENS heuristic_rens(model);
    //model.addHeuristic(&heuristic_rens);
    //CbcHeuristicRINS heuristic_rins(model);
    //model.addHeuristic(&heuristic_rins);
    //CbcRounding heuristic_rounding(model);
    //model.addHeuristic(&heuristic_rounding);
    //CbcSerendipity heuristic_serendipity(model);
    //model.addHeuristic(&heuristic_serendipity);

    // Cuts
    //CglClique cutgen_clique;
    //model.addCutGenerator(&cutgen_clique);
    //CglAllDifferent cutgen_alldifferent;
    //model.addCutGenerator(&cutgen_alldifferent);
    //CglDuplicateRow cutgen_duplicaterow;
    //model.addCutGenerator(&cutgen_duplicaterow);
    //CglFlowCover cutgen_flowcover;
    //model.addCutGenerator(&cutgen_flowcover);
    //CglGomory cutgen_gomory;
    //model.addCutGenerator(&cutgen_gomory);
    //CglKnapsackCover cutgen_knapsackcover;
    //model.addCutGenerator(&cutgen_knapsackcover);
    //CglLandP cutgen_landp;
    //model.addCutGenerator(&cutgen_landp);
    //CglLiftAndProject cutgen_liftandproject;
    //model.addCutGenerator(&cutgen_liftandproject);
    //CglMixedIntegerRounding cutgen_mixedintegerrounding;
    //model.addCutGenerator(&cutgen_mixedintegerrounding);
    //CglMixedIntegerRounding2 cutgen_mixedintegerrounding2;
    //model.addCutGenerator(&cutgen_mixedintegerrounding2);
    //CglOddHole cutgen_oddhole;
    //model.addCutGenerator(&cutgen_oddhole);
    //CglProbing cutgen_probing;
    //model.addCutGenerator(&cutgen_probing);
    //CglRedSplit cutgen_redsplit;
    //model.addCutGenerator(&cutgen_redsplit);
    //CglResidualCapacity cutgen_residualcapacity;
    //model.addCutGenerator(&cutgen_residualcapacity);
    //CglSimpleRounding cutgen_simplerounding;
    //model.addCutGenerator(&cutgen_simplerounding);
    //CglStored cutgen_stored;
    //model.addCutGenerator(&cutgen_stored);
    //CglTwomir cutgen_twomir;
    //model.addCutGenerator(&cutgen_twomir);

    // Set time limit
    model.setMaximumSeconds(p.info.remaining_time());

    // Add initial solution
    std::vector<double> sol_init(instance.alternative_number(), 0);
    if (p.initial_solution != NULL && p.initial_solution->feasible()) {
        for (AltIdx k = 0; k < instance.alternative_number(); ++k)
            if (p.initial_solution->agent(instance.alternative(k).j) == instance.alternative(k).i)
                sol_init[k] = 1;
        model.setBestSolution(sol_init.data(), instance.alternative_number(), p.initial_solution->cost());
    }

    // Stop af first improvment
    if (p.stop_at_first_improvment)
        model.setMaximumSolutions(1);

    // Do complete search
    model.branchAndBound();

    if (model.isProvenInfeasible()) {
        output.update_lower_bound(instance.bound(), std::stringstream(""), p.info);
    } else if (model.isProvenOptimal()) {
        if (!output.solution.feasible() || output.solution.cost() > model.getObjValue() + 0.5) {
            const double *solution = model.solver()->getColSolution();
            Solution sol_curr(instance);
            for (AltIdx k = 0; k < instance.alternative_number(); ++k)
                if (solution[k] > 0.5)
                    sol_curr.set(k);
            output.update_solution(sol_curr, std::stringstream(""), p.info);
        }
        output.update_lower_bound(output.solution.cost(), std::stringstream(""), p.info);
    } else if (model.bestSolution() != NULL) {
        if (!output.solution.feasible() || output.solution.cost() > model.getObjValue() + 0.5) {
            const double *solution = model.solver()->getColSolution();
            Solution sol_curr(instance);
            for (AltIdx k = 0; k < instance.alternative_number(); ++k)
                if (solution[k] > 0.5)
                    sol_curr.set(k);
            output.update_solution(sol_curr, std::stringstream(""), p.info);
        }
        Cost lb = std::ceil(model.getBestPossibleObjValue() - TOL);
        output.update_lower_bound(lb, std::stringstream(""), p.info);
    } else {
        Cost lb = std::ceil(model.getBestPossibleObjValue() - TOL);
        output.update_lower_bound(lb, std::stringstream(""), p.info);
    }

    return output.algorithm_end(p.info);
}

#endif

