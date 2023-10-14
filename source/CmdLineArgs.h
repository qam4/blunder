/*
    CmdLineArgs.h
*/

#ifndef CMD_LINE_ARGS_H
#define CMD_LINE_ARGS_H

#include <algorithm>
#include <string>
#include <vector>

class CmdLineArgs
{
  public:
    CmdLineArgs(const int argc, char** argv);
    const std::string get_cmd_option(const std::string& option) const;
    bool cmd_option_exists(const std::string& option) const;
    const std::string& get_program_name() const { return tokens_[0]; };

  private:
    std::vector<std::string> tokens_;
};

#endif  // CMD_LINE_ARGS_H
