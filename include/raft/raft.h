/**
 * @file raft.h
 * @brief Umbrella header — include this single file to get the full Raft API.
 */

#pragma once

// Core
#include "raft/core/cluster_membership.h"
#include "raft/core/configuration.h"
#include "raft/core/raft_log.h"
#include "raft/core/raft_node.h"
#include "raft/core/state_machine.h"

// RPC / networking
#include "raft/rpc/messages.h"
#include "raft/rpc/transport.h"

// Storage
#include "raft/storage/log_store.h"
#include "raft/storage/persistent_state.h"
#include "raft/storage/snapshot_store.h"

// Utilities
#include "raft/util/logger.h"
#include "raft/util/metrics.h"
#include "raft/util/serializer.h"
#include "raft/util/timer.h"
#include "raft/util/types.h"
