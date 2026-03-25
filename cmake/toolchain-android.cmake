# Android NDK cross-compilation toolchain for Blunder.
#
# This file wraps the NDK's built-in CMake toolchain and sets
# Blunder-specific defaults.  The NDK toolchain is included
# automatically when CMAKE_SYSTEM_NAME is "Android" and the NDK
# path is provided.
#
# Required variables (set via preset or command line):
#   ANDROID_NDK_HOME  or  CMAKE_ANDROID_NDK   — path to NDK root
#   CMAKE_ANDROID_ARCH_ABI                     — target ABI
#
# Usage:
#   cmake -S . -B build/android-arm64 \
#     --preset android-arm64
#
# Or manually:
#   cmake -S . -B build/android-arm64 \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-android.cmake \
#     -DCMAKE_ANDROID_NDK=$ANDROID_NDK_HOME \
#     -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
#     -DCMAKE_BUILD_TYPE=Release

cmake_minimum_required(VERSION 3.21)

set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 24)  # Android 7.0 — wide device coverage

# Resolve NDK path: prefer CMAKE_ANDROID_NDK, fall back to env var.
if(NOT CMAKE_ANDROID_NDK)
  if(DEFINED ENV{ANDROID_NDK_HOME})
    set(CMAKE_ANDROID_NDK "$ENV{ANDROID_NDK_HOME}")
  elseif(DEFINED ENV{ANDROID_NDK_ROOT})
    set(CMAKE_ANDROID_NDK "$ENV{ANDROID_NDK_ROOT}")
  elseif(DEFINED ENV{ANDROID_NDK})
    set(CMAKE_ANDROID_NDK "$ENV{ANDROID_NDK}")
  endif()
endif()

if(NOT CMAKE_ANDROID_NDK)
  message(FATAL_ERROR
    "Android NDK not found.  Set CMAKE_ANDROID_NDK, ANDROID_NDK_HOME, "
    "ANDROID_NDK_ROOT, or ANDROID_NDK environment variable to the NDK root.")
endif()

# Default to arm64-v8a if not specified.
if(NOT CMAKE_ANDROID_ARCH_ABI)
  set(CMAKE_ANDROID_ARCH_ABI "arm64-v8a")
endif()

# Use clang (the only supported NDK compiler since r18).
set(CMAKE_ANDROID_STL_TYPE "c++_static")
