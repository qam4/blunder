/*
 * File:   Log.h
 *
 * Simple logging utility for runtime warnings and debug output.
 * Uses the xboard "# " comment prefix so output doesn't confuse the GUI.
 */

#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <string>

namespace Log
{

inline void warning(const std::string& msg)
{
    std::cerr << "# WARNING: " << msg << std::endl;
}

inline void error(const std::string& msg)
{
    std::cerr << "# ERROR: " << msg << std::endl;
}

inline void debug([[maybe_unused]] const std::string& msg)
{
#ifdef DEBUG
    std::cerr << "# DEBUG: " << msg << std::endl;
#endif
}

}  // namespace Log

#endif /* LOG_H */
