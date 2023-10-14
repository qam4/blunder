# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location


include(FetchContent)
include(CTest)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.0.1 # or a later release
)

# CMake 3.14+
FetchContent_MakeAvailable(Catch2)

project(blunderTests LANGUAGES CXX)

# ---- Tests ----

add_executable(blunder_test
    source/TestBoard.cpp
    source/TestSearch.cpp
    source/TestParser.cpp
    source/TestMove.cpp
    source/TestMoveList.cpp
    source/TestMoveGenerator.cpp
    source/TestZobrist.cpp
    source/TestPerft.cpp
    source/TestTestPositions.cpp
    source/TestsRunner.cpp
    )
target_link_libraries(blunder_test PRIVATE blunder_lib)
target_link_libraries(blunder_test PRIVATE Catch2::Catch2WithMain)
target_compile_features(blunder_test PRIVATE cxx_std_17)

include(CTest)
include(Catch)
catch_discover_tests(blunder_test)

# ---- End-of-file commands ----

add_folders(Test)
