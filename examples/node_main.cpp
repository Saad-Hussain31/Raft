// =============================================================================
// Raft Node Example
// =============================================================================
//
// A minimal Raft node executable that demonstrates how to wire together all
// of the library's components and start a single node in a Raft cluster.
//
// Usage:
//   raft_node --id <N> --port <P> --peers <id1:host1:port1,id2:host2:port2,...>
//             --data-dir <path>
//
// Example: launching a 3-node cluster on localhost
// ------------------------------------------------
//   Terminal 1:
//     ./raft_node --id 1 --port 5001
//       --peers 2:127.0.0.1:5002,3:127.0.0.1:5003
//       --data-dir /tmp/raft-node-1
//
//   Terminal 2:
//     ./raft_node --id 2 --port 5002
//       --peers 1:127.0.0.1:5001,3:127.0.0.1:5003
//       --data-dir /tmp/raft-node-2
//
//   Terminal 3:
//     ./raft_node --id 3 --port 5003
//       --peers 1:127.0.0.1:5001,2:127.0.0.1:5002
//       --data-dir /tmp/raft-node-3
//
// Each node will participate in leader election and replicate log entries.
// You can stop any node with Ctrl-C (SIGINT) for a graceful shutdown.
//
// =============================================================================

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// --------------------------------------------------------------------------
// Raft library headers
// --------------------------------------------------------------------------
#include <raft/core/cluster_membership.h>   // Peer / cluster membership types
#include <raft/core/configuration.h>        // Configuration (election timeout, etc.)
#include <raft/core/raft_node.h>            // RaftNode — the main entry point
#include <raft/core/state_machine.h>        // StateMachine interface
#include <raft/raft.h>                      // Convenience umbrella header
#include <raft/rpc/messages.h>              // RPC message types
#include <raft/rpc/transport.h>             // Transport interface (TCP impl)
#include <raft/storage/log_store.h>         // LogStore interface
#include <raft/storage/persistent_state.h>  // PersistentState interface
#include <raft/storage/snapshot_store.h>    // SnapshotStore interface
#include <raft/util/logger.h>               // Logger (spdlog-based)
#include <raft/util/metrics.h>              // Metrics collector
#include <raft/util/serializer.h>           // Serializer (JSON-based)
#include <raft/util/timer.h>                // Timer (real wall-clock impl)
#include <raft/util/types.h>                // Common type aliases

// =============================================================================
// KeyValueStateMachine
// =============================================================================
//
// A trivial "echo" key-value store that serves as the replicated state machine.
// In a real application you would replace this with your own domain logic
// (database engine, coordination service, distributed lock manager, etc.).
//
// Commands are expected as simple strings of the form:
//   SET <key> <value>
//   GET <key>
//   DEL <key>
//
// The state is an in-memory std::unordered_map<std::string, std::string>.
// =============================================================================

namespace raft {

/**
 * @brief A trivial key-value StateMachine for demonstration.
 *
 * Matches the StateMachine interface declared in include/raft/core/state_machine.h.
 * Commands are opaque byte vectors; this implementation interprets them as
 * "SET key value", "GET key", or "DEL key" text commands.
 */
class KeyValueStateMachine : public StateMachine {
   public:
    std::vector<uint8_t> apply(LogIndex index, Term /*term*/, EntryType type,
                                std::span<const uint8_t> data) override {
        // TODO: Parse command bytes and apply to the in-memory store.
        //
        // For Noop and Config entries, return empty result.
        // For Command entries, interpret data as "SET key value" / "GET key" / "DEL key".
        last_applied_ = index;
        if (type != EntryType::Command) return {};

        std::string cmd(data.begin(), data.end());
        std::istringstream stream(cmd);
        std::string op;
        stream >> op;

        if (op == "SET") {
            std::string key, value;
            stream >> key;
            std::getline(stream >> std::ws, value);
            store_[key] = value;
            std::string r = "OK";
            return {r.begin(), r.end()};
        }
        if (op == "GET") {
            std::string key;
            stream >> key;
            auto it = store_.find(key);
            std::string r = (it != store_.end()) ? it->second : "(nil)";
            return {r.begin(), r.end()};
        }
        if (op == "DEL") {
            std::string key;
            stream >> key;
            std::string r = store_.erase(key) > 0 ? "1" : "0";
            return {r.begin(), r.end()};
        }
        std::string r = "ERR unknown command";
        return {r.begin(), r.end()};
    }

    std::vector<uint8_t> take_snapshot() const override {
        // TODO: Serialize the store.
        std::ostringstream oss;
        for (const auto& [k, v] : store_) oss << k << "=" << v << "\n";
        auto s = oss.str();
        return {s.begin(), s.end()};
    }

    Result<void> restore_snapshot(std::span<const uint8_t> data) override {
        // TODO: Deserialize the store from snapshot bytes.
        store_.clear();
        std::string s(data.begin(), data.end());
        std::istringstream stream(s);
        std::string line;
        while (std::getline(stream, line)) {
            auto pos = line.find('=');
            if (pos != std::string::npos)
                store_[line.substr(0, pos)] = line.substr(pos + 1);
        }
        return Result<void>::success();
    }

    LogIndex last_applied() const override { return last_applied_; }

   private:
    std::unordered_map<std::string, std::string> store_;
    LogIndex last_applied_{LogIndex::none()};
};

}  // namespace raft

// =============================================================================
// Command-line argument parsing
// =============================================================================

struct NodeArgs {
    std::uint64_t id = 0;    // This node's unique ID in the cluster
    std::uint16_t port = 0;  // TCP port to listen on
    std::string data_dir;    // Directory for persistent state & logs

    // Peer list: each entry is (id, host, port).
    struct PeerInfo {
        std::uint64_t id;
        std::string host;
        std::uint16_t port;
    };
    std::vector<PeerInfo> peers;
};

// ---------------------------------------------------------------------------
// Print usage information and exit.
// ---------------------------------------------------------------------------
[[noreturn]] static void print_usage_and_exit(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [OPTIONS]\n"
              << "\n"
              << "Required options:\n"
              << "  --id <N>            Unique numeric node ID (positive integer)\n"
              << "  --port <P>          TCP port this node listens on\n"
              << "  --peers <list>      Comma-separated peer list:\n"
              << "                        id1:host1:port1,id2:host2:port2,...\n"
              << "  --data-dir <path>   Directory for persistent storage\n"
              << "\n"
              << "Example:\n"
              << "  " << program_name
              << " --id 1 --port 5001 --peers 2:127.0.0.1:5002,3:127.0.0.1:5003"
              << " --data-dir /tmp/raft-node-1\n";
    std::exit(1);
}

// ---------------------------------------------------------------------------
// Parse the --peers string into a vector of PeerInfo structs.
// Expected format: "id1:host1:port1,id2:host2:port2,..."
// ---------------------------------------------------------------------------
static std::vector<NodeArgs::PeerInfo> parse_peers(const std::string& peers_str) {
    std::vector<NodeArgs::PeerInfo> result;
    std::istringstream stream(peers_str);
    std::string token;

    while (std::getline(stream, token, ',')) {
        // Each token should be "id:host:port".
        std::istringstream entry(token);
        std::string id_str, host, port_str;

        if (!std::getline(entry, id_str, ':') || !std::getline(entry, host, ':') ||
            !std::getline(entry, port_str, ':')) {
            std::cerr << "ERROR: malformed peer entry: \"" << token << "\"\n";
            std::exit(1);
        }

        NodeArgs::PeerInfo peer;
        peer.id = static_cast<std::uint64_t>(std::stoull(id_str));
        peer.host = host;
        peer.port = static_cast<std::uint16_t>(std::stoi(port_str));
        result.push_back(std::move(peer));
    }

    return result;
}

// ---------------------------------------------------------------------------
// Parse all command-line arguments into a NodeArgs struct.
// ---------------------------------------------------------------------------
static NodeArgs parse_args(int argc, char* argv[]) {
    NodeArgs args;

    for (int i = 1; i < argc; ++i) {
        std::string flag(argv[i]);

        // Helper: ensure there is a next argument after the flag.
        auto next_arg = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "ERROR: " << flag << " requires an argument\n";
                print_usage_and_exit(argv[0]);
            }
            return argv[++i];
        };

        if (flag == "--id") {
            args.id = static_cast<std::uint64_t>(std::stoull(next_arg()));
        } else if (flag == "--port") {
            args.port = static_cast<std::uint16_t>(std::stoi(next_arg()));
        } else if (flag == "--peers") {
            args.peers = parse_peers(next_arg());
        } else if (flag == "--data-dir") {
            args.data_dir = next_arg();
        } else if (flag == "--help" || flag == "-h") {
            print_usage_and_exit(argv[0]);
        } else {
            std::cerr << "ERROR: unknown flag: " << flag << "\n";
            print_usage_and_exit(argv[0]);
        }
    }

    // Validate required arguments.
    if (args.id == 0) {
        std::cerr << "ERROR: --id is required and must be > 0\n";
        print_usage_and_exit(argv[0]);
    }
    if (args.port == 0) {
        std::cerr << "ERROR: --port is required\n";
        print_usage_and_exit(argv[0]);
    }
    if (args.data_dir.empty()) {
        std::cerr << "ERROR: --data-dir is required\n";
        print_usage_and_exit(argv[0]);
    }
    if (args.peers.empty()) {
        std::cerr << "ERROR: --peers is required (at least one peer)\n";
        print_usage_and_exit(argv[0]);
    }

    return args;
}

// =============================================================================
// Signal handling — allows graceful shutdown on Ctrl-C / kill
// =============================================================================

namespace {
std::atomic<bool> g_shutdown_requested{false};
std::condition_variable g_shutdown_cv;
std::mutex g_shutdown_mutex;
}  // namespace

static void signal_handler(int /*signum*/) {
    g_shutdown_requested.store(true, std::memory_order_release);
    g_shutdown_cv.notify_all();
}

// =============================================================================
// main
// =============================================================================

int main(int argc, char* argv[]) {
    // -----------------------------------------------------------------------
    // 1. Parse command-line arguments
    // -----------------------------------------------------------------------
    NodeArgs args = parse_args(argc, argv);

    std::cout << "Starting Raft node " << args.id << " on port " << args.port
              << " with data-dir: " << args.data_dir << "\n";

    // -----------------------------------------------------------------------
    // 2. Set up signal handling for graceful shutdown
    // -----------------------------------------------------------------------
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // -----------------------------------------------------------------------
    // 3. Create the Configuration
    //    This holds tunables such as election timeout range, heartbeat
    //    interval, max log entries per AppendEntries RPC, etc.
    // -----------------------------------------------------------------------
    // TODO: Expose configuration knobs via additional CLI flags or a config
    //       file. For now we rely on sensible defaults from the library.
    raft::Configuration config;
    config.node_id = raft::NodeId{args.id};
    config.listen_address = {"0.0.0.0", args.port};
    config.data_dir = args.data_dir;

    // -----------------------------------------------------------------------
    // 4. Build initial peer list
    // -----------------------------------------------------------------------
    for (const auto& peer : args.peers) {
        config.initial_peers.push_back({raft::NodeId{peer.id}, {peer.host, peer.port}});
    }
    // Include self in the peer list
    config.initial_peers.push_back({config.node_id, config.listen_address});

    // -----------------------------------------------------------------------
    // 5. Create all components using factory functions
    // -----------------------------------------------------------------------
    auto logger = raft::create_spdlog_logger(config.node_id, raft::LogLevel::Info);
    auto shared_logger = std::shared_ptr<raft::Logger>(std::move(logger));
    auto serializer = raft::create_json_serializer();
    auto metrics = raft::create_in_memory_metrics();
    auto timer_factory = raft::create_real_timer_factory();
    auto transport = raft::create_tcp_transport(shared_logger);

    auto persistent_state = raft::create_file_persistent_state(args.data_dir + "/state");
    auto log_store = raft::create_file_log_store(args.data_dir + "/log");
    auto snapshot_store = raft::create_file_snapshot_store(args.data_dir + "/snapshots");
    auto state_machine = std::make_unique<raft::KeyValueStateMachine>();

    // -----------------------------------------------------------------------
    // 6. Create and start the RaftNode
    // -----------------------------------------------------------------------
    // TODO: Uncomment once all stubs are implemented:
    // raft::RaftNode node(
    //     std::move(config),
    //     std::move(state_machine),
    //     std::move(transport),
    //     std::move(persistent_state),
    //     std::move(log_store),
    //     std::move(snapshot_store),
    //     std::move(timer_factory),
    //     std::move(serializer),
    //     shared_logger,
    //     std::move(metrics)
    // );
    // auto result = node.start();
    // if (!result.ok()) {
    //     std::cerr << "Failed to start node\n";
    //     return 1;
    // }

    std::cout << "Raft node " << args.id << " is running. Press Ctrl-C to stop.\n";

    // -----------------------------------------------------------------------
    // 13. Block until a shutdown signal is received
    // -----------------------------------------------------------------------
    {
        std::unique_lock<std::mutex> lock(g_shutdown_mutex);
        g_shutdown_cv.wait(lock,
                           [] { return g_shutdown_requested.load(std::memory_order_acquire); });
    }

    // -----------------------------------------------------------------------
    // 14. Graceful shutdown
    // -----------------------------------------------------------------------
    std::cout << "Shutting down Raft node " << args.id << "...\n";

    // TODO: Call node.stop() to drain in-flight RPCs, flush persistent state,
    //       and cleanly leave the cluster.
    //
    // node.stop();

    std::cout << "Raft node " << args.id << " stopped.\n";
    return 0;
}
