/*
 * File:   TestPositions.h
 * Author: fm
 *
 */

#ifndef TEST_POSITIONS_H
#define TEST_POSITIONS_H

#include <map>
#include <string>

#include "Common.h"
#include "Board.h"

// Search mode for test positions
enum class SearchMode
{
    FixedNodes,  // Original: max 1M nodes
    FixedDepth,  // Fixed depth (e.g., depth 8)
    FixedTime,   // Fixed time per position in milliseconds
};

// Per-category result for STS
struct CategoryResult
{
    int score = 0;
    int max_score = 0;
    int positions = 0;
};

// Full benchmark result
struct BenchmarkResult
{
    double score_pct = 0.0;
    int score = 0;
    int max_score = 0;
    int elo = 0;
    long long total_nodes = 0;
    double total_time_secs = 0.0;
    int nps = 0;
    std::map<std::string, CategoryResult> categories;  // STS per-category
};

double test_positions(std::string path_to_epd);
double test_positions_benchmark(std::string path_to_epd);

BenchmarkResult test_positions_ex(const std::string& path_to_epd,
                                  SearchMode mode,
                                  int param);  // depth or time_ms or nodes

#endif /* TEST_POSITIONS_H */
