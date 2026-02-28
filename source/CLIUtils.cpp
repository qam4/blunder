/*
    CLIUtils.cpp
*/

#include <sstream>

#include "CLIUtils.h"

using std::getline;
using std::istringstream;
using std::string;
using std::vector;

// Split string into a vector of strings
vector<string> split(const string& s, const char delimiter)
{
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

int str2int(const string& s)
{
    size_t pos = 0;
    size_t len = s.length();
    int value = 0;
    while (s[pos] >= '0' && s[pos] <= '9' && pos < len)
    {
        value = value * 10 + (s[pos] - '0');
        pos++;
    }
    return value;
}
