/*
 * File:   InputDetect.cpp
 *
 * Non-blocking stdin input detection for pondering.
 * Uses PeekNamedPipe / PeekConsoleInput on Windows and select() on POSIX.
 */

#include "InputDetect.h"

#ifdef _WIN32

#    include <io.h>
#    include <windows.h>

bool input_available()
{
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD file_type = GetFileType(h);

    if (file_type == FILE_TYPE_PIPE)
    {
        // Redirected stdin (pipe from GUI or tournament manager)
        DWORD avail = 0;
        if (PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr))
        {
            return avail > 0;
        }
        return false;
    }

    // Console input: check for pending key-down events
    INPUT_RECORD rec;
    DWORD count = 0;
    while (PeekConsoleInput(h, &rec, 1, &count) && count > 0)
    {
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown)
        {
            return true;
        }
        // Discard non-key events (mouse, resize, etc.)
        ReadConsoleInput(h, &rec, 1, &count);
    }
    return false;
}

#else  // POSIX

#    include <sys/select.h>
#    include <unistd.h>

bool input_available()
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = { 0, 0 };
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
}

#endif
