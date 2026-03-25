# iOS cross-compilation toolchain for Blunder.
#
# Builds a universal (arm64) static library for iOS devices.
# Requires Xcode and the iOS SDK installed on macOS.
#
# Usage:
#   cmake -S . -B build/ios \
#     --preset ios
#
# Or manually:
#   cmake -S . -B build/ios \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-ios.cmake \
#     -DCMAKE_BUILD_TYPE=Release
#
# For simulator (x86_64 / arm64 Mac):
#   cmake -S . -B build/ios-sim \
#     --preset ios-simulator

cmake_minimum_required(VERSION 3.21)

set(CMAKE_SYSTEM_NAME iOS)

# Minimum deployment target — iOS 15 covers 98%+ of active devices.
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum iOS version")

# Build for arm64 (all modern iPhones/iPads).
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "iOS architectures")

# Let CMake find the iOS SDK automatically.
# Override with -DCMAKE_OSX_SYSROOT=<path> if needed.

# Disable bitcode (deprecated since Xcode 14).
set(CMAKE_XCODE_ATTRIBUTE_ENABLE_BITCODE "NO")
