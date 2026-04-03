# cmake/CompilerWarnings.cmake
# ──────────────────────────────────────────────────────────────────────────────
# Applies a strict set of compiler warnings project-wide.
# These flags catch common bugs, undefined behaviour, and style issues early.
# ──────────────────────────────────────────────────────────────────────────────

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
)

# GCC-specific warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
    )
endif()
