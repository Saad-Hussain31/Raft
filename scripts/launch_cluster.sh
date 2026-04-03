#!/usr/bin/env bash
# --------------------------------------------------------------------------
# launch_cluster.sh
#
# Launches a local Raft cluster of N nodes on localhost.
# Each node listens on port 9001 + (node_id - 1) and stores data in
# /tmp/raft_node_<id>/.
#
# Usage:
#   ./scripts/launch_cluster.sh          # default: 3 nodes
#   ./scripts/launch_cluster.sh 5        # 5 nodes on ports 9001-9005
#
# All nodes are launched as background processes. Press Ctrl+C to kill them
# all. The script traps SIGINT and SIGTERM for clean shutdown.
#
# NOTE: Make this script executable before first use:
#   chmod +x scripts/launch_cluster.sh
# --------------------------------------------------------------------------

set -euo pipefail

# --- Configuration --------------------------------------------------------

NUM_NODES="${1:-3}"
BASE_PORT=9001
HOST="127.0.0.1"
DATA_DIR_PREFIX="/tmp/raft_node"
LOG_LEVEL="debug"

# Path to the node binary (relative to project root)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
NODE_BIN="${PROJECT_ROOT}/build/examples/raft_node"

# --- Validation -----------------------------------------------------------

if [[ ! -x "${NODE_BIN}" ]]; then
    echo "ERROR: Node binary not found at ${NODE_BIN}"
    echo ""
    echo "Build the project first:"
    echo "  mkdir -p build && cd build"
    echo "  cmake .. -DCMAKE_BUILD_TYPE=Debug -DRAFT_BUILD_EXAMPLES=ON"
    echo "  cmake --build . -j\$(nproc)"
    exit 1
fi

if [[ "${NUM_NODES}" -lt 1 ]]; then
    echo "ERROR: Number of nodes must be at least 1."
    exit 1
fi

# --- Build peer list ------------------------------------------------------

PEERS=""
for i in $(seq 1 "${NUM_NODES}"); do
    PORT=$((BASE_PORT + i - 1))
    if [[ -n "${PEERS}" ]]; then
        PEERS="${PEERS},"
    fi
    PEERS="${PEERS}${i}=${HOST}:${PORT}"
done

echo "============================================================"
echo " Raft Cluster Launcher"
echo "============================================================"
echo " Nodes:      ${NUM_NODES}"
echo " Ports:      ${BASE_PORT} - $((BASE_PORT + NUM_NODES - 1))"
echo " Host:       ${HOST}"
echo " Peers:      ${PEERS}"
echo " Log level:  ${LOG_LEVEL}"
echo " Data dirs:  ${DATA_DIR_PREFIX}_1 ... ${DATA_DIR_PREFIX}_${NUM_NODES}"
echo "============================================================"
echo ""

# --- Create data directories ----------------------------------------------

for i in $(seq 1 "${NUM_NODES}"); do
    mkdir -p "${DATA_DIR_PREFIX}_${i}"
done

# --- Track PIDs for cleanup -----------------------------------------------

PIDS=()

cleanup() {
    echo ""
    echo "Shutting down cluster..."
    for pid in "${PIDS[@]}"; do
        if kill -0 "${pid}" 2>/dev/null; then
            echo "  Killing node (PID ${pid})"
            kill "${pid}" 2>/dev/null || true
        fi
    done
    # Give processes a moment to exit, then force-kill
    sleep 1
    for pid in "${PIDS[@]}"; do
        if kill -0 "${pid}" 2>/dev/null; then
            echo "  Force-killing node (PID ${pid})"
            kill -9 "${pid}" 2>/dev/null || true
        fi
    done
    echo "All nodes stopped."
}

trap cleanup SIGINT SIGTERM EXIT

# --- Launch nodes ---------------------------------------------------------

for i in $(seq 1 "${NUM_NODES}"); do
    PORT=$((BASE_PORT + i - 1))
    DATA_DIR="${DATA_DIR_PREFIX}_${i}"

    echo "Starting node ${i} on ${HOST}:${PORT} (data: ${DATA_DIR})"

    "${NODE_BIN}" \
        --id "${i}" \
        --listen "${HOST}:${PORT}" \
        --peers "${PEERS}" \
        --data-dir "${DATA_DIR}" \
        --log-level "${LOG_LEVEL}" \
        &

    PIDS+=($!)
done

echo ""
echo "All ${NUM_NODES} nodes started. PIDs: ${PIDS[*]}"
echo "Press Ctrl+C to stop the cluster."
echo ""

# --- Wait for all processes -----------------------------------------------

# Wait for any child to exit. If one crashes, report it.
while true; do
    for idx in "${!PIDS[@]}"; do
        pid="${PIDS[$idx]}"
        node_id=$((idx + 1))
        if ! kill -0 "${pid}" 2>/dev/null; then
            wait "${pid}" 2>/dev/null || true
            EXIT_CODE=$?
            echo "WARNING: Node ${node_id} (PID ${pid}) exited with code ${EXIT_CODE}"
            # Remove from array to avoid repeated warnings
            unset 'PIDS[idx]'
        fi
    done

    # If all nodes have exited, stop
    if [[ ${#PIDS[@]} -eq 0 ]]; then
        echo "All nodes have exited."
        break
    fi

    sleep 1
done
