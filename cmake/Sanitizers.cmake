# cmake/Sanitizers.cmake
# ──────────────────────────────────────────────────────────────────────────────
# Enables AddressSanitizer and UndefinedBehaviorSanitizer when
# RAFT_ENABLE_SANITIZERS is ON.  These are invaluable for catching memory
# errors, data races, and undefined behaviour during development and CI.
# ──────────────────────────────────────────────────────────────────────────────

if(RAFT_ENABLE_SANITIZERS)
    message(STATUS "Sanitizers enabled (ASan + UBSan)")

    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()
