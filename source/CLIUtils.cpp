/*
    CLIUtils.cpp
*/

#include <sstream>

#include "CLIUtils.h"
using namespace std;

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
