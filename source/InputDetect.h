/*
 * File:   InputDetect.h
 *
 * Non-blocking stdin input detection for pondering.
 * Uses PeekNamedPipe on Windows and select() on POSIX.
 */

#ifndef INPUTDETECT_H
#define INPUTDETECT_H

/// Returns true if there is data waiting on stdin (non-blocking).
bool input_available();

#endif  // INPUTDETECT_H
