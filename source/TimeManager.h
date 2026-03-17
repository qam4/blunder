/*
 * File:   TimeManager.h
 *
 * Owns search timing state: start time, time budget, and node limits.
 * Extracted from Board to separate time management from game state.
 *
 * Smart time management (Req 28):
 *   - allocate() computes soft and hard time limits from clock state
 *   - Instability detection: extend time when best move changes
 *   - Score-based adjustment: reduce time when clearly winning/losing
 *   - Hard limit is never exceeded
 *
 * Uses std::chrono::steady_clock for wall-time measurement (not CPU time).
 * This is critical for UCI time management where wtime/btime are wall-clock.
 */

#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include <algorithm>
#include <chrono>

#include "Constants.h"

class TimeManager
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    /// Allocate time for a move based on clock state.
    /// @param time_left_cs  Remaining time in centiseconds
    /// @param inc_cs        Increment per move in centiseconds
    /// @param moves_to_go   Moves until next time control (0 = sudden death)
    void allocate(int time_left_cs, int inc_cs, int moves_to_go)
    {
        // Convert to microseconds for internal use
        int time_left_us = time_left_cs * 10000;
        int inc_us = inc_cs * 10000;

        // Estimate moves remaining: use moves_to_go if set, otherwise assume 30
        int moves_est = (moves_to_go > 0) ? (std::max)(moves_to_go, 1) : 30;

        // Base time: fraction of remaining time + 3/4 of increment
        int base = time_left_us / moves_est + inc_us * 3 / 4;

        // Safety margin: never use more than 90% of remaining time
        int max_allowed = time_left_us * 9 / 10;

        // Ensure max_allowed is at least a small amount if we have any time
        if (max_allowed < 100000 && time_left_us > 100000)
        {
            max_allowed = 100000;  // 0.1s floor
        }

        // Clamp base to max_allowed
        if (base > max_allowed)
        {
            base = max_allowed;
        }

        // Ensure base is at least 0.1s if we have time
        if (base < 100000 && time_left_us > 100000)
        {
            base = 100000;
        }
        else if (base < 0)
        {
            base = 100000;
        }

        soft_limit_ = base;

        // Hard limit: 2x soft limit, but never more than 1/4 of remaining time
        int quarter_time = time_left_us / 4;
        hard_limit_ = (std::min)(soft_limit_ * 2, (std::min)(max_allowed, quarter_time));
        if (hard_limit_ < soft_limit_)
        {
            hard_limit_ = soft_limit_;
        }

        start_ = Clock::now();
        max_nodes_ = -1;
        score_adjusted_ = false;
        easy_move_applied_ = false;
    }

    /// Legacy start method for non-clock-based searches (fixed time, node limit).
    void start(int search_time, int max_nodes = -1)
    {
        search_time_ = search_time;
        soft_limit_ = search_time;
        hard_limit_ = search_time;
        max_nodes_ = max_nodes;
        start_ = Clock::now();
        score_adjusted_ = false;
        easy_move_applied_ = false;
    }

    /// Extend soft limit when best move is unstable between iterations.
    void extend_for_instability()
    {
        soft_limit_ = soft_limit_ * 3 / 2;
        if (soft_limit_ > hard_limit_)
        {
            soft_limit_ = hard_limit_;
        }
    }

    /// Reduce soft limit when position is clearly won or lost.
    /// @param score_cp  Score in centipawns from side-to-move perspective
    void adjust_for_score(int score_cp)
    {
        if (!score_adjusted_ && (score_cp > 500 || score_cp < -500))
        {
            soft_limit_ = soft_limit_ * 3 / 5;
            score_adjusted_ = true;
        }
    }

    /// Reduce soft limit when best move is stable (easy move detection).
    /// Called once when the best move hasn't changed for several iterations.
    /// Only applies when a real time limit is set (soft_limit_ > 0).
    void reduce_for_easy_move()
    {
        if (!easy_move_applied_ && soft_limit_ > 0)
        {
            soft_limit_ = soft_limit_ / 2;
            easy_move_applied_ = true;
        }
    }

    /// Elapsed wall time in microseconds since start/allocate.
    int elapsed_us() const
    {
        auto now = Clock::now();
        return static_cast<int>(
            std::chrono::duration_cast<std::chrono::microseconds>(now - start_).count());
    }

    /// Called every 2048 nodes in alphabeta/quiesce to check hard time limit.
    bool is_time_over(int nodes_visited) const
    {
        if ((max_nodes_ != -1) && (nodes_visited > max_nodes_))
        {
            return true;
        }

        int limit = hard_limit_;
        if (limit == -1)
        {
            return false;
        }

        return (elapsed_us() > limit);
    }

    /// Called between iterative deepening iterations to decide whether
    /// there is enough time for another depth.
    bool should_stop(int nodes_visited) const
    {
        if ((max_nodes_ != -1) && (nodes_visited > max_nodes_))
        {
            return true;
        }

        int limit = soft_limit_;
        if (limit == -1)
        {
            return false;
        }

        return (elapsed_us() > limit);
    }

    TimePoint start_time() const { return start_; }
    int search_time() const { return search_time_; }
    int soft_limit() const { return soft_limit_; }
    int hard_limit() const { return hard_limit_; }

private:
    TimePoint start_ = Clock::now();
    int search_time_ = DEFAULT_SEARCH_TIME;
    int soft_limit_ = DEFAULT_SEARCH_TIME;
    int hard_limit_ = DEFAULT_SEARCH_TIME;
    int max_nodes_ = -1;
    bool score_adjusted_ = false;
    bool easy_move_applied_ = false;
};

#endif /* TIMEMANAGER_H */
