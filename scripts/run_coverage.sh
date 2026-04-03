#!/usr/bin/env bash
# --------------------------------------------------------------------------
# run_coverage.sh
#
# Builds the project with coverage instrumentation, runs all tests, captures
# coverage data with lcov, filters out third-party and test code, generates
# an HTML report, and optionally opens it in a browser.
#
# Usage:
#   ./scripts/run_coverage.sh            # build, test, report, open
#   ./scripts/run_coverage.sh --no-open  # skip opening the browser
#
# Prerequisites:
#   - GCC or Clang (coverage flags: --coverage, -fprofile-arcs, -ftest-coverage)
#   - lcov (sudo apt install lcov  /  brew install lcov)
#   - genhtml (included with lcov)
#
# NOTE: Make this script executable before first use:
#   chmod +x scripts/run_coverage.sh
# --------------------------------------------------------------------------

set -euo pipefail

# --- Configuration --------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-coverage"
COVERAGE_RAW="${BUILD_DIR}/coverage.info"
COVERAGE_FILTERED="${BUILD_DIR}/coverage_filtered.info"
COVERAGE_HTML="${BUILD_DIR}/coverage_html"

OPEN_BROWSER=true
if [[ "${1:-}" == "--no-open" ]]; then
    OPEN_BROWSER=false
fi

# --- Check prerequisites --------------------------------------------------

for cmd in cmake lcov genhtml; do
    if ! command -v "${cmd}" &>/dev/null; then
        echo "ERROR: '${cmd}' not found. Please install it."
        if [[ "${cmd}" == "lcov" || "${cmd}" == "genhtml" ]]; then
            echo "  Ubuntu/Debian: sudo apt install lcov"
            echo "  macOS:         brew install lcov"
        fi
        exit 1
    fi
done

# --- Step 1: Configure with coverage -------------------------------------

echo "============================================================"
echo " Step 1: Configuring with coverage instrumentation"
echo "============================================================"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake "${PROJECT_ROOT}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DRAFT_ENABLE_COVERAGE=ON \
    -DRAFT_BUILD_TESTS=ON \
    -DRAFT_BUILD_EXAMPLES=OFF

# --- Step 2: Build -------------------------------------------------------

echo ""
echo "============================================================"
echo " Step 2: Building"
echo "============================================================"

cmake --build . -j"$(nproc)"

# --- Step 3: Reset counters -----------------------------------------------

echo ""
echo "============================================================"
echo " Step 3: Resetting coverage counters"
echo "============================================================"

lcov --zerocounters --directory .

# Capture baseline (before tests)
lcov --capture --initial \
    --directory . \
    --output-file "${COVERAGE_RAW}.baseline" \
    --rc lcov_branch_coverage=1 \
    2>/dev/null || true

# --- Step 4: Run tests ----------------------------------------------------

echo ""
echo "============================================================"
echo " Step 4: Running tests"
echo "============================================================"

ctest --test-dir . --output-on-failure || {
    echo ""
    echo "WARNING: Some tests failed. Coverage report will be partial."
    echo ""
}

# --- Step 5: Capture coverage ---------------------------------------------

echo ""
echo "============================================================"
echo " Step 5: Capturing coverage data"
echo "============================================================"

lcov --capture \
    --directory . \
    --output-file "${COVERAGE_RAW}" \
    --rc lcov_branch_coverage=1

# --- Step 6: Filter -------------------------------------------------------

echo ""
echo "============================================================"
echo " Step 6: Filtering out third-party and test code"
echo "============================================================"

lcov --remove "${COVERAGE_RAW}" \
    '/usr/*' \
    '*/third_party/*' \
    '*/tests/*' \
    '*/build-coverage/*' \
    '*/googletest/*' \
    '*/spdlog/*' \
    '*/nlohmann/*' \
    --output-file "${COVERAGE_FILTERED}" \
    --rc lcov_branch_coverage=1

# --- Step 7: Generate HTML report ----------------------------------------

echo ""
echo "============================================================"
echo " Step 7: Generating HTML report"
echo "============================================================"

genhtml "${COVERAGE_FILTERED}" \
    --output-directory "${COVERAGE_HTML}" \
    --title "Raft Coverage Report" \
    --legend \
    --branch-coverage \
    --rc lcov_branch_coverage=1

echo ""
echo "============================================================"
echo " Coverage report generated at:"
echo "   ${COVERAGE_HTML}/index.html"
echo "============================================================"

# --- Step 8: Summary ------------------------------------------------------

echo ""
echo "--- Summary ---"
lcov --summary "${COVERAGE_FILTERED}" --rc lcov_branch_coverage=1 2>&1 || true

# --- Step 9: Open in browser (optional) -----------------------------------

if [[ "${OPEN_BROWSER}" == true ]]; then
    echo ""
    echo "Opening report in browser..."

    if command -v xdg-open &>/dev/null; then
        xdg-open "${COVERAGE_HTML}/index.html" 2>/dev/null &
    elif command -v open &>/dev/null; then
        open "${COVERAGE_HTML}/index.html"
    elif command -v wslview &>/dev/null; then
        wslview "${COVERAGE_HTML}/index.html"
    else
        echo "Could not detect a browser opener. Open manually:"
        echo "  ${COVERAGE_HTML}/index.html"
    fi
fi
