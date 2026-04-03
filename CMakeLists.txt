cmake_minimum_required(VERSION 3.22)

project(raft
    VERSION 0.1.0
    DESCRIPTION "Production-grade Raft consensus algorithm implementation"
    LANGUAGES CXX
)

# ─── Global settings ──────────────────────────────────────────────────────────

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Options ──────────────────────────────────────────────────────────────────

option(RAFT_BUILD_TESTS      "Build unit and integration tests"  ON)
option(RAFT_BUILD_EXAMPLES   "Build example applications"        ON)
option(RAFT_ENABLE_COVERAGE  "Enable code coverage instrumentation (gcc/clang)" OFF)
option(RAFT_ENABLE_SANITIZERS "Enable ASan + UBSan"              OFF)

# ─── cmake/ modules ──────────────────────────────────────────────────────────

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)
include(Coverage)

# ─── Third-party dependencies ────────────────────────────────────────────────

# Google Test — used only for tests
if(RAFT_BUILD_TESTS)
    add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)
endif()

# spdlog — structured logging
add_subdirectory(third_party/spdlog EXCLUDE_FROM_ALL)

# nlohmann/json — serialization
set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(third_party/json EXCLUDE_FROM_ALL)

# ─── Core library ────────────────────────────────────────────────────────────

add_subdirectory(src)

# ─── Examples ─────────────────────────────────────────────────────────────────

if(RAFT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ─── Tests ────────────────────────────────────────────────────────────────────

if(RAFT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
