cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    blunder
    VERSION 0.1.0
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
    source/Common.cpp
    source/Move.cpp
    source/MoveGenerator.cpp
    source/MoveList.cpp
    source/Output.cpp
    source/Parser.cpp
    source/MiniMax.cpp
    source/NegaMax.cpp
    source/AlphaBeta.cpp
    source/Quiesce.cpp
    source/Zobrist.cpp
    source/ValidateMove.cpp
    source/Hash.cpp
    source/PrincipalVariation.cpp
)

target_include_directories(
    blunder_lib ${warning_guard}
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(blunder_lib PUBLIC cxx_std_17)

# ---- Declare executable ----

add_executable(blunder_exe source/main.cpp)
add_executable(blunder::exe ALIAS blunder_exe)

set_property(TARGET blunder_exe PROPERTY OUTPUT_NAME blunder)

target_compile_features(blunder_exe PRIVATE cxx_std_17)

target_link_libraries(blunder_exe PRIVATE blunder_lib)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT blunder_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of blunder"
  )
endif()

include(cmake/dev-mode.cmake)
