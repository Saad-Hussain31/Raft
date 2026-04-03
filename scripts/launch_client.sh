#!/usr/bin/env bash
# --------------------------------------------------------------------------
# launch_client.sh
#
# Launches the Raft client binary, pointing at a node on localhost.
#
# Usage:
#   ./scripts/launch_client.sh                 # default: connect to node 1 (port 9001)
#   ./scripts/launch_client.sh 9002            # connect to port 9002
#   ./scripts/launch_client.sh 127.0.0.1:9003  # connect to specific address
#
# The client will automatically redirect to the leader if the contacted node
# is a follower (using the leader_hint in the response).
#
# NOTE: Make this script executable before first use:
#   chmod +x scripts/launch_client.sh
# --------------------------------------------------------------------------

set -euo pipefail

# --- Configuration --------------------------------------------------------

DEFAULT_TARGET="127.0.0.1:9001"
TARGET="${1:-${DEFAULT_TARGET}}"

# If only a port number was given, prepend localhost
if [[ "${TARGET}" =~ ^[0-9]+$ ]]; then
    TARGET="127.0.0.1:${TARGET}"
fi

# Path to the client binary (relative to project root)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLIENT_BIN="${PROJECT_ROOT}/build/examples/raft_client"

# --- Validation -----------------------------------------------------------

if [[ ! -x "${CLIENT_BIN}" ]]; then
    echo "ERROR: Client binary not found at ${CLIENT_BIN}"
    echo ""
    echo "Build the project first:"
    echo "  mkdir -p build && cd build"
    echo "  cmake .. -DCMAKE_BUILD_TYPE=Debug -DRAFT_BUILD_EXAMPLES=ON"
    echo "  cmake --build . -j\$(nproc)"
    exit 1
fi

# --- Launch ---------------------------------------------------------------

echo "============================================================"
echo " Raft Client"
echo "============================================================"
echo " Target:  ${TARGET}"
echo "============================================================"
echo ""

exec "${CLIENT_BIN}" --target "${TARGET}"
