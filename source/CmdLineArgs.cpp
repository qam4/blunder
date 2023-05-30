/*
    CmdLineArgs.cpp
*/

#include "CmdLineArgs.h"

CmdLineArgs::CmdLineArgs(const int argc, char** argv)
{
    for (int i = 0; i < argc; ++i)
    {
        tokens.push_back(std::string(argv[i]));
    }
}

const std::string CmdLineArgs::get_cmd_option(const std::string& option) const
{
    auto itr = std::find(tokens.begin(), tokens.end(), option);
    if (itr != tokens.end() && ++itr != tokens.end())
    {
        return *(itr);
    }
    return std::string();
}

bool CmdLineArgs::cmd_option_exists(const std::string& option) const
{
    return std::find(tokens.begin(), tokens.end(), option) != tokens.end();
}
