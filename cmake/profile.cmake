# include(FetchContent)

# FetchContent_Declare(
#   FlameGraph
#   GIT_REPOSITORY https://github.com/brendangregg/FlameGraph.git
#   GIT_TAG        v1.0 # or a later release
#   CONFIGURE_COMMAND ""  # tell CMake it's not a cmake project
#   BUILD_COMMAND ""
# )

# # CMake 3.14+
# FetchContent_MakeAvailable(FlameGraph)


# ---- Variables ----

# We use variables separate from what CTest uses, because those have
# customization issues
set(
    PROFILE_RECORD_COMMAND
    perf
    record
    --call-graph dwarf
    $<TARGET_FILE:blunder_exe>
    --perft
    CACHE STRING
    "; separated command to generate a record for the 'profile' target"
)

set(
    PROFILE_REPORT_COMMAND
    perf report --stdio > "${PROJECT_BINARY_DIR}/perf-report.out"
    CACHE STRING
    "; separated command to generate a report for the 'profile' target"
)

set(
    PROFILE_FLAMEGRAPH_COMMAND
    perf script -i ${PROJECT_BINARY_DIR}/perf.data | $ENV{HOME}/src/FlameGraph/stackcollapse-perf.pl | $ENV{HOME}/src/FlameGraph/flamegraph.pl > ${PROJECT_BINARY_DIR}/perf.svg
    CACHE STRING
    "; separated command to generate a flamegraph for the 'profile' target"
)



# ---- Profile target ----

add_custom_target(
    profile
    COMMAND ${PROFILE_RECORD_COMMAND}
    COMMAND ${PROFILE_REPORT_COMMAND}
    COMMAND ${PROFILE_FLAMEGRAPH_COMMAND}
    COMMENT "Generating profile report"
    VERBATIM
)
add_dependencies(profile blunder_exe)
# add_dependencies(profile FlameGraph)
