# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location


include(FetchContent)
include(CTest)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)

# Edit flags for catch2
if (LINUX)
  set(CMAKE_CXX_FLAGS_OLD "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-conversion -Wno-sign-conversion -Wno-double-promotion")
endif()
if(DEFINED CMAKE_CXX_CPPCHECK)
  set(CMAKE_CXX_CPPCHECK_OLD "${CMAKE_CXX_CPPCHECK}")
  set(CMAKE_CXX_CPPCHECK "${CMAKE_CXX_CPPCHECK};--suppress=missingReturn")
endif()


FetchContent_MakeAvailable(Catch2)

# restore flags
if (LINUX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_OLD}")
endif()
if(DEFINED CMAKE_CXX_CPPCHECK)
  set(CMAKE_CXX_CPPCHECK "${CMAKE_CXX_CPPCHECK_OLD}")
endif()

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
    source/TestSee.cpp
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
