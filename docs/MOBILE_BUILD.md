# Building Blunder for Mobile Platforms

Blunder cross-compiles to Android and iOS as a static library (`libblunder.a`)
that your mobile app links against. The engine communicates via stdin/stdout
(UCI + coaching protocol), so the typical integration pattern is to spawn the
engine in a background thread and pipe commands to it.

## Quick Start (recommended)

The build script handles everything — including downloading the Android NDK
if you don't have one:

```bash
# Build all available targets (Android on any OS, iOS on macOS only)
./scripts/build-mobile.sh

# Android only
./scripts/build-mobile.sh android

# Single target
./scripts/build-mobile.sh android-arm64

# iOS only (macOS required)
./scripts/build-mobile.sh ios
```

Output goes to `build/mobile-dist/`:
```
build/mobile-dist/blunder-android-arm64.zip    # engine/blunder + books/*.bin
build/mobile-dist/blunder-android-x86_64.zip
build/mobile-dist/blunder-ios-arm64.zip        # lib/libblunder.a + include/*.h (macOS only)
build/mobile-dist/blunder-ios-simulator.zip    # macOS only
```

Android zip contents (what chess-coach expects):
```
engine/blunder          # ARM64 ELF executable (~1MB stripped)
books/i-gm1950.bin      # Opening book (19MB)
books/k-stfish.bin      # Opening book (5.5MB)
books/n-larsen.bin      # Opening book (991KB)
books/openings.epd      # Opening positions (23KB)
weights/                # NNUE weights (if present)
```

The chess-coach Android wrapper extracts these to `{APP_DATA}/engine/` and
spawns `blunder --uci` as a subprocess.

The script downloads the NDK to `prefix/ndk/` (gitignored) on first run.
Set `ANDROID_NDK_HOME` to skip the download if you already have one.

## CI

Mobile builds run automatically on every push/PR via GitHub Actions.
Tagged releases include mobile artifacts alongside desktop binaries.
See `.github/workflows/ci.yml` (the `mobile` job).

## Prerequisites (manual builds only)

| Platform | Requirement |
|----------|-------------|
| Android  | [Android NDK r25+](https://developer.android.com/ndk/downloads) and CMake 3.21+ |
| iOS      | Xcode 14+ with iOS SDK, CMake 3.21+, macOS host |

## Android (manual)

### 1. Install the NDK

Via Android Studio SDK Manager, or download directly, or let the build
script handle it:

```bash
# Example: set the env var to your NDK location
export ANDROID_NDK_HOME=$HOME/Android/Sdk/ndk/27.0.12077973
```

### 2. Configure and build

```bash
# arm64 (all modern phones)
cmake --preset=android-arm64
cmake --build build/android-arm64

# x86_64 (emulator)
cmake --preset=android-x86_64
cmake --build build/android-x86_64
```

### 3. Output

```
build/android-arm64/libblunder.a    # static library
source/*.h                          # headers (install with cmake --install)
```

### 4. Integration with Android app

Add to your `CMakeLists.txt` in the Android project:

```cmake
add_library(blunder STATIC IMPORTED)
set_target_properties(blunder PROPERTIES
  IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libblunder.a
  INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/include/blunder)
target_link_libraries(your_jni_lib PRIVATE blunder)
```

Or use `add_subdirectory()` to build blunder as part of your app's CMake build.

## iOS (manual)

### 1. Configure and build (from macOS)

```bash
# Device (arm64)
cmake --preset=ios
cmake --build build/ios

# Simulator (arm64 + x86_64 universal)
cmake --preset=ios-simulator
cmake --build build/ios-sim
```

### 2. Output

```
build/ios/libblunder.a       # device static library
build/ios-sim/libblunder.a   # simulator static library
```

### 3. Create XCFramework (optional, recommended)

To bundle device + simulator into a single distributable:

```bash
xcodebuild -create-xcframework \
  -library build/ios/libblunder.a -headers source/ \
  -library build/ios-sim/libblunder.a -headers source/ \
  -output build/Blunder.xcframework
```

### 4. Integration with Xcode project

Drag `Blunder.xcframework` (or `libblunder.a` + headers) into your Xcode
project. Add the header search path and link the library.

## Integration Pattern for chess-coach

The recommended integration for a mobile chess-coach app:

1. Spawn a background thread that runs the engine's main loop
2. Redirect stdin/stdout to pipes controlled by your app
3. Send UCI commands + coaching protocol commands over the pipe
4. Parse responses (UCI info lines + `BEGIN_COACH_RESPONSE` / `END_COACH_RESPONSE`)

Alternatively, for tighter integration, you can call the engine's C++ classes
directly by linking `libblunder.a` and using the `Board`, `Search`,
`CoachDispatcher`, etc. classes from the headers.

## Architecture Notes

- The engine is pure C++17 with no external dependencies beyond pthreads
- `InputDetect.cpp` uses POSIX `select()` on Android/iOS (works out of the box)
- The `-mpopcnt` flag is only applied on x86_64 — ARM builds use the compiler's
  built-in popcount which maps to hardware instructions on ARMv8+
- Android minimum API level is 24 (Android 7.0)
- iOS minimum deployment target is 15.0

## Supported ABIs

| Platform | ABI | Preset | Notes |
|----------|-----|--------|-------|
| Android  | arm64-v8a | `android-arm64` | All modern phones (99%+ of active devices) |
| Android  | x86_64 | `android-x86_64` | Emulator testing |
| iOS      | arm64 | `ios` | All iPhones since 5s |
| iOS Sim  | arm64 + x86_64 | `ios-simulator` | Universal simulator binary |
