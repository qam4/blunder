/*
 * File:   MCTS.h
 *
 * Monte Carlo Tree Search implementation.
 * Uses UCB1 for selection, neural network (or hand-crafted) evaluation
 * at leaf nodes, and backpropagation of value estimates.
 */

#ifndef MCTS_H
#define MCTS_H

#include <cmath>
#include <memory>
#include <vector>

#include "Board.h"
#include "Evaluator.h"
#include "Move.h"
#include "TimeManager.h"

class DualHeadNetwork;

/// A single node in the MCTS tree.
struct MCTSNode
{
    Move_t move;                              // Move that led to this node
    MCTSNode* parent = nullptr;               // Parent node (nullptr for root)
    std::vector<std::unique_ptr<MCTSNode>> children;
    int visits = 0;
    double total_value = 0.0;                 // Sum of backpropagated values
    double prior = 0.0;                       // Prior probability (uniform if no policy net)

    explicit MCTSNode(Move_t m = Move(0), MCTSNode* p = nullptr)
        : move(m)
        , parent(p)
    {
    }

    /// Upper Confidence Bound for Trees (UCT).
    /// Returns +infinity for unvisited nodes to ensure exploration.
    double ucb(double c_puct) const
    {
        if (visits == 0)
        {
            return 1e9;
        }
        double exploitation = total_value / visits;
        double exploration =
            c_puct * prior * std::sqrt(static_cast<double>(parent->visits)) / (1.0 + visits);
        return exploitation + exploration;
    }

    /// Average value of this node (from the perspective of the player who moved here).
    double avg_value() const
    {
        return (visits > 0) ? total_value / visits : 0.0;
    }

    bool is_leaf() const { return children.empty(); }
};

/// MCTS search engine. Operates on a Board using an Evaluator for leaf values.
class MCTS
{
public:
    /// @param board      Game state (will be modified during search, restored after)
    /// @param evaluator  Leaf evaluation function
    /// @param c_puct     Exploration constant (default 1.41 ≈ sqrt(2) for UCB1)
    /// @param simulations Number of simulations per search call
    MCTS(Board& board, Evaluator& evaluator, double c_puct = 1.41, int simulations = 800);

    /// Construct with a dual-head neural network for policy priors and value evaluation.
    /// When a DualHeadNetwork is provided, expand() uses the policy head for priors
    /// and simulate() uses the value head for leaf evaluation.
    MCTS(Board& board, DualHeadNetwork& network, double c_puct = 1.41, int simulations = 800);

    /// Run MCTS and return the best move.
    /// @param tm  Optional time manager for time-based termination
    Move_t search(TimeManager* tm = nullptr);

    /// Run MCTS and return the root node (caller owns the tree).
    /// Use this to inspect visit counts for training data generation.
    std::unique_ptr<MCTSNode> search_return_root(TimeManager* tm = nullptr);

    /// Accessors
    int get_simulations() const { return simulations_; }
    void set_simulations(int n) { simulations_ = n; }
    double get_c_puct() const { return c_puct_; }
    void set_c_puct(double c) { c_puct_ = c; }
    int get_nodes_visited() const { return nodes_visited_; }

private:
    /// Select a leaf node by traversing the tree using UCB.
    /// Applies moves to board_ as it descends. Returns the path depth.
    MCTSNode* select(MCTSNode* root, int& depth);

    /// Expand a leaf node by generating all legal moves as children.
    void expand(MCTSNode* node);

    /// Evaluate the current board position (leaf evaluation).
    /// Returns value in [-1, 1] from the perspective of the side that just moved.
    double simulate(MCTSNode* node);

    /// Backpropagate the evaluation result up the tree.
    /// Alternates sign at each level (negamax-style).
    void backpropagate(MCTSNode* node, double value);

    Board& board_;
    Evaluator* evaluator_;
    DualHeadNetwork* network_ = nullptr;  // null = use handcrafted eval
    double c_puct_;
    int simulations_;
    int nodes_visited_ = 0;
};

#endif /* MCTS_H */
