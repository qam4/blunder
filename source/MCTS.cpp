/*
 * File:   MCTS.cpp
 *
 * Monte Carlo Tree Search implementation.
 */

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#include "MCTS.h"

#include "MoveGenerator.h"
#include "MoveList.h"

MCTS::MCTS(Board& board, Evaluator& evaluator, double c_puct, int simulations)
    : board_(board)
    , evaluator_(evaluator)
    , c_puct_(c_puct)
    , simulations_(simulations)
{
}

Move_t MCTS::search(TimeManager* tm)
{
    auto root = search_return_root(tm);

    // Select the child with the most visits (robust child selection)
    MCTSNode* best = nullptr;
    int best_visits = -1;
    for (auto& child : root->children)
    {
        if (child->visits > best_visits)
        {
            best_visits = child->visits;
            best = child.get();
        }
    }

    return best ? best->move : Move(0);
}

std::unique_ptr<MCTSNode> MCTS::search_return_root(TimeManager* tm)
{
    nodes_visited_ = 0;

    // Create root node
    auto root = std::make_unique<MCTSNode>();

    // Expand root immediately
    expand(root.get());

    if (root->children.empty())
    {
        return root;
    }

    // Run simulations
    for (int i = 0; i < simulations_; ++i)
    {
        // Check time limit every 64 simulations
        if (tm != nullptr && (i & 63) == 0 && i > 0)
        {
            if (tm->is_time_over(nodes_visited_))
            {
                break;
            }
        }

        // Selection: descend tree using UCB
        int depth = 0;
        MCTSNode* leaf = select(root.get(), depth);

        // Expansion: if leaf has been visited, expand it
        if (leaf->visits > 0 && !board_.is_game_over())
        {
            expand(leaf);
            if (!leaf->children.empty())
            {
                // Pick first child (unvisited, will have UCB = +inf)
                leaf = leaf->children[0].get();
                board_.do_move(leaf->move);
                ++depth;
            }
        }

        // Simulation (leaf evaluation)
        double value = simulate(leaf);

        // Backpropagation
        backpropagate(leaf, value);

        // Undo moves applied during selection/expansion
        for (int d = 0; d < depth; ++d)
        {
            board_.undo_move(leaf->move);
            leaf = leaf->parent;
        }
    }

    return root;
}

MCTSNode* MCTS::select(MCTSNode* root, int& depth)
{
    MCTSNode* node = root;
    while (!node->is_leaf())
    {
        // Pick child with highest UCB
        MCTSNode* best_child = nullptr;
        double best_ucb = -std::numeric_limits<double>::infinity();
        for (auto& child : node->children)
        {
            double u = child->ucb(c_puct_);
            if (u > best_ucb)
            {
                best_ucb = u;
                best_child = child.get();
            }
        }
        assert(best_child != nullptr);
        board_.do_move(best_child->move);
        ++depth;
        node = best_child;
    }
    return node;
}

void MCTS::expand(MCTSNode* node)
{
    MoveList moves;
    MoveGenerator::add_all_moves(moves, board_, board_.side_to_move());

    int num_legal = moves.length();
    if (num_legal == 0)
    {
        return;
    }

    // Uniform prior — no policy network yet
    double uniform_prior = 1.0 / num_legal;

    node->children.reserve(static_cast<size_t>(num_legal));
    for (int i = 0; i < num_legal; ++i)
    {
        auto child = std::make_unique<MCTSNode>(moves[i], node);
        child->prior = uniform_prior;
        node->children.push_back(std::move(child));
    }

    nodes_visited_ += num_legal;
}

double MCTS::simulate(MCTSNode* node)
{
    (void)node;

    // Check for terminal positions
    if (board_.is_game_over())
    {
        if (board_.is_draw())
        {
            return 0.0;
        }
        // Checkmate — the side to move is mated, so the side that just moved wins
        return 1.0;
    }

    // Leaf evaluation using the evaluator (NNUE or hand-crafted)
    // side_relative_eval returns score from side-to-move perspective
    int score_cp = evaluator_.side_relative_eval(board_);

    // Convert centipawn score to [-1, 1] using a sigmoid-like mapping
    // tanh(score / 400) maps roughly: ±100cp → ±0.24, ±300cp → ±0.64, ±600cp → ±0.93
    double value = std::tanh(static_cast<double>(score_cp) / 400.0);

    // Negate because side_relative_eval is from current side's perspective,
    // but we want the value from the perspective of the side that just moved
    // (the parent's side).
    return -value;
}

void MCTS::backpropagate(MCTSNode* node, double value)
{
    MCTSNode* current = node;
    double v = value;
    while (current != nullptr)
    {
        current->visits++;
        current->total_value += v;
        v = -v;  // Flip value at each level (negamax convention)
        current = current->parent;
    }
}
