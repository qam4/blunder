cmake_minimum_required(VERSION 3.21)

include(cmake/prelude.cmake)

project(
    blunder
    VERSION 0.8.0
    DESCRIPTION "Chess engine"
    HOMEPAGE_URL "https://example.com/"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(
    blunder_lib OBJECT
    source/Board.cpp
    source/Book.cpp
    source/CLIConfig.cpp
    source/CLIUtils.cpp
    source/DualHeadNetwork.cpp
    source/MCTS.cpp
    source/CmdLineArgs.cpp
    source/Common.cpp
    source/InputDetect.cpp
    source/Move.cpp
    source/MoveGenerator.cpp
    source/MoveGeneratorHyperbola.cpp
    source/MoveGeneratorMagicBitboards.cpp
    source/MoveList.cpp
    source/Output.cpp
    source/Parser.cpp
    source/Search.cpp
    source/SelfPlay.cpp
    source/Zobrist.cpp
    source/ValidateMove.cpp
    source/PrincipalVariation.cpp
    source/TranspositionTable.cpp
    source/Perft.cpp
    source/TestPositions.cpp
    source/UCI.cpp
    source/Xboard.cpp
    source/See.cpp
    source/Evaluator.cpp
    source/NNUEEvaluator.cpp
    source/CoachJson.cpp
    source/CoachDispatcher.cpp
    source/PositionAnalyzer.cpp
    source/MoveComparator.cpp
)

target_include_directories(
    blunder_lib ${warning_guard}
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(blunder_lib PUBLIC cxx_std_17)

# Enable hardware POPCNT on x86_64 (GCC/Clang).  Without this flag
# __builtin_popcountll compiles to a library call (__popcountdi2) instead
# of the single-instruction POPCNT.  Profiling shows this costs ~6.6% of
# total search time.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(blunder_lib PRIVATE -mpopcnt)
  endif()
endif()

# ---- Mobile / embedded build options ----

option(BLUNDER_BUILD_EXE "Build the blunder executable" ON)
option(BLUNDER_BUILD_STATIC_LIB
  "Build blunder as a static library (for mobile app embedding)" OFF)

# When cross-compiling for iOS, default to static lib only.
# Android builds produce an executable (spawned via subprocess).
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(BLUNDER_BUILD_EXE OFF CACHE BOOL "" FORCE)
  set(BLUNDER_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)
endif()

# Android: build executable, link statically so it has no runtime deps.
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(BLUNDER_BUILD_EXE ON CACHE BOOL "" FORCE)
  set(BLUNDER_BUILD_STATIC_LIB OFF CACHE BOOL "" FORCE)
  # Static link libc++ and libm so the binary runs standalone on any device.
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++" CACHE STRING "" FORCE)
endif()

find_package(Threads REQUIRED)
target_link_libraries(blunder_lib PUBLIC Threads::Threads)

target_compile_definitions(
    blunder_lib
    PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)

# ---- Declare static library (for mobile embedding) ----

if(BLUNDER_BUILD_STATIC_LIB)
  add_library(blunder_static STATIC $<TARGET_OBJECTS:blunder_lib>)
  target_include_directories(blunder_static PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>")
  target_compile_features(blunder_static PUBLIC cxx_std_17)
  target_link_libraries(blunder_static PUBLIC Threads::Threads)
  set_property(TARGET blunder_static PROPERTY OUTPUT_NAME blunder)

  install(TARGETS blunder_static
    ARCHIVE DESTINATION lib COMPONENT blunder_Runtime)
  install(DIRECTORY source/
    DESTINATION include/blunder
    COMPONENT blunder_Development
    FILES_MATCHING PATTERN "*.h")
endif()

# ---- Declare executable ----

if(BLUNDER_BUILD_EXE)
  add_executable(blunder_exe source/main.cpp)
  add_executable(blunder::exe ALIAS blunder_exe)

  set_property(TARGET blunder_exe PROPERTY OUTPUT_NAME blunder)

  target_compile_features(blunder_exe PRIVATE cxx_std_17)

  target_link_libraries(blunder_exe PRIVATE blunder_lib)
endif()

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES AND BLUNDER_BUILD_EXE)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT blunder_DEVELOPER_MODE OR NOT BLUNDER_BUILD_EXE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of blunder"
  )
endif()

include(cmake/dev-mode.cmake)
