# cmake/Coverage.cmake
# ──────────────────────────────────────────────────────────────────────────────
# Adds --coverage flags when RAFT_ENABLE_COVERAGE is ON.
# After building and running tests, generate a report with:
#
#   scripts/run_coverage.sh
#
# Or manually:
#   lcov --capture --directory . --output-file coverage.info
#   lcov --remove coverage.info '/usr/*' 'third_party/*' 'tests/*' -o filtered.info
#   genhtml filtered.info --output-directory coverage_html
# ──────────────────────────────────────────────────────────────────────────────

if(RAFT_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(WARNING "Coverage is only supported with GCC or Clang")
        return()
    endif()

    message(STATUS "Code coverage instrumentation enabled")

    add_compile_options(--coverage -fprofile-arcs -ftest-coverage -O0 -g)
    add_link_options(--coverage)
endif()
