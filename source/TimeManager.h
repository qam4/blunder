/*
 * File:   TimeManager.h
 *
 * Owns search timing state: start time, time budget, and node limits.
 * Extracted from Board to separate time management from game state.
 */

#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include <ctime>

#include "Constants.h"

class TimeManager
{
public:
    void start(int search_time, int max_nodes = -1)
    {
        search_time_ = search_time;
        max_nodes_ = max_nodes;
        start_time_ = clock();
    }

    // Called every 2048 nodes in alphabeta/quiesce to check hard time limit.
    // search_time_ is in microseconds; -1 means infinite.
    bool is_time_over(int nodes_visited) const
    {
        if ((max_nodes_ != -1) && (nodes_visited > max_nodes_))
        {
            return true;
        }

        if (search_time_ == -1)
        {
            return false;
        }

        clock_t current_time = clock();
        int elapsed_time = int((1000000 * double(current_time - start_time_)) / CLOCKS_PER_SEC);

        return (elapsed_time > search_time_);
    }

    // Called between iterative deepening iterations to decide whether
    // there is enough time for another depth. Uses the heuristic that
    // the next iteration will take ~2x the elapsed time so far.
    bool should_stop(int nodes_visited) const
    {
        if ((max_nodes_ != -1) && (nodes_visited > max_nodes_))
        {
            return true;
        }

        if (search_time_ == -1)
        {
            return false;
        }

        clock_t current_time = clock();
        clock_t elapsed_time = current_time - start_time_;

        // https://mediocrechess.blogspot.com/2007/01/guide-time-management.html
        // check to see if we have enough time left to search
        // for 2 times the last depth's time
        return (2 * elapsed_time > search_time_);
    }

    clock_t start_time() const { return start_time_; }
    int search_time() const { return search_time_; }

private:
    clock_t start_time_ = 0;
    int search_time_ = DEFAULT_SEARCH_TIME;
    int max_nodes_ = -1;
};

#endif /* TIMEMANAGER_H */
