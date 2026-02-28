/*
    CLIUtils.h
*/

#ifndef CLI_UTILS_H
#define CLI_UTILS_H

#include <string>
#include <vector>

std::vector<std::string> split(const std::string& s, const char delimiter);
int str2int(const std::string& s);

#endif  // CLI_UTILS_H
