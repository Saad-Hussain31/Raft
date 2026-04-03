# Third-Party Dependencies

This directory contains Git submodules for the project's external dependencies. The submodules point to upstream repositories and are pinned to specific commits for reproducible builds.

## Dependencies

| Directory      | Library            | Purpose                           | URL                                           |
|----------------|--------------------|-----------------------------------|-----------------------------------------------|
| `googletest/`  | Google Test 1.14+  | Unit and integration testing framework | https://github.com/google/googletest.git |
| `spdlog/`      | spdlog 1.12+       | Fast, header-only structured logging   | https://github.com/gabime/spdlog.git     |
| `json/`        | nlohmann/json 3.11+| JSON serialization/deserialization     | https://github.com/nlohmann/json.git     |

## Initialization

After cloning the repository, initialize and fetch the submodules:

```bash
git submodule update --init --recursive
```

If you originally cloned without submodules:

```bash
# From the project root:
git submodule init
git submodule update --recursive
```

## Updating to Newer Versions

To update a submodule to a newer commit (e.g., for a security patch):

```bash
cd third_party/spdlog
git fetch origin
git checkout v1.13.0   # or desired tag/commit
cd ../..
git add third_party/spdlog
git commit -m "Update spdlog to v1.13.0"
```

## Build Integration

These submodules are consumed by the top-level `CMakeLists.txt` via `add_subdirectory()`:

- **googletest**: Only built when `RAFT_BUILD_TESTS=ON` (default). Excluded from the main target via `EXCLUDE_FROM_ALL`.
- **spdlog**: Linked as `spdlog::spdlog`. Built as a static library. Excluded from install via `EXCLUDE_FROM_ALL`.
- **json**: Linked as `nlohmann_json::nlohmann_json`. Tests are disabled via `JSON_BuildTests=OFF`.

## Troubleshooting

### Empty Directories

If the submodule directories exist but are empty, the submodules were not initialized:

```bash
git submodule update --init --recursive
```

### CMake Cannot Find Submodule

If CMake fails with an error like `add_subdirectory given source ... which is not an existing directory`, the submodule is missing:

```bash
git submodule update --init --recursive
```

### Detached HEAD Warnings

Submodules are always checked out in detached HEAD state. This is normal and expected. Do not commit changes directly inside submodule directories.
