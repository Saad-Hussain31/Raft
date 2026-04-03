// =============================================================================
// Raft Client Example
// =============================================================================
//
// A simple interactive client that connects to a Raft cluster node and sends
// key-value commands (SET, GET, DEL).  When the contacted node is not the
// leader, the client follows the leader_hint redirect automatically.
//
// Usage:
//   raft_client --host <H> --port <P>
//
// Example:
//   ./raft_client --host 127.0.0.1 --port 5001
//
// Once connected you can type commands interactively:
//   > SET mykey hello
//   OK
//   > GET mykey
//   hello
//   > DEL mykey
//   1
//   > quit
//
// The client will keep retrying with leader redirection until it finds the
// current leader or a maximum retry count is reached.
//
// =============================================================================

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

// --------------------------------------------------------------------------
// Raft library headers
// --------------------------------------------------------------------------
#include <raft/raft.h>             // Convenience umbrella header
#include <raft/rpc/messages.h>     // ClientRequest, ClientResponse, etc.
#include <raft/rpc/transport.h>    // Transport interface
#include <raft/util/serializer.h>  // JSON serializer for wire format
#include <raft/util/types.h>       // Common type aliases

// =============================================================================
// Command-line argument parsing
// =============================================================================

struct ClientArgs {
    std::string host = "127.0.0.1";  // Default to localhost
    std::uint16_t port = 5001;       // Default Raft node port
};

// ---------------------------------------------------------------------------
// Print usage information and exit.
// ---------------------------------------------------------------------------
[[noreturn]] static void print_usage_and_exit(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [OPTIONS]\n"
              << "\n"
              << "Options:\n"
              << "  --host <H>   Host of the Raft node to connect to (default: 127.0.0.1)\n"
              << "  --port <P>   TCP port of the Raft node (default: 5001)\n"
              << "\n"
              << "Interactive commands:\n"
              << "  SET <key> <value>   Store a key-value pair\n"
              << "  GET <key>           Retrieve the value for a key\n"
              << "  DEL <key>           Delete a key\n"
              << "  quit / exit         Disconnect and exit\n";
    std::exit(1);
}

// ---------------------------------------------------------------------------
// Parse command-line arguments into a ClientArgs struct.
// ---------------------------------------------------------------------------
static ClientArgs parse_args(int argc, char* argv[]) {
    ClientArgs args;

    for (int i = 1; i < argc; ++i) {
        std::string flag(argv[i]);

        auto next_arg = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "ERROR: " << flag << " requires an argument\n";
                print_usage_and_exit(argv[0]);
            }
            return argv[++i];
        };

        if (flag == "--host") {
            args.host = next_arg();
        } else if (flag == "--port") {
            args.port = static_cast<std::uint16_t>(std::stoi(next_arg()));
        } else if (flag == "--help" || flag == "-h") {
            print_usage_and_exit(argv[0]);
        } else {
            std::cerr << "ERROR: unknown flag: " << flag << "\n";
            print_usage_and_exit(argv[0]);
        }
    }

    return args;
}

// =============================================================================
// Client connection and RPC helpers
// =============================================================================

namespace raft {

// ---------------------------------------------------------------------------
// RaftClient — encapsulates connecting to a Raft node, sending ClientRequest
// RPCs, and handling leader redirection.
//
// TODO: This is a stub. The actual implementation needs to:
//   1. Open a TCP connection to the specified host:port.
//   2. Serialize a ClientRequest message (containing the command string).
//   3. Send the serialized bytes over the TCP socket.
//   4. Read and deserialize the ClientResponse.
//   5. If the response indicates NotLeader and includes a leader_hint,
//      reconnect to the hinted leader and retry the request.
// ---------------------------------------------------------------------------
class RaftClient {
   public:
    RaftClient(std::string host, std::uint16_t port)
        : current_host_(std::move(host)), current_port_(port) {
        // TODO: Initialize the serializer for encoding/decoding messages.
        // serializer_ = std::make_shared<Serializer>();
    }

    ~RaftClient() { disconnect(); }

    // -----------------------------------------------------------------------
    // connect() — establish a TCP connection to the current host:port.
    // -----------------------------------------------------------------------
    bool connect() {
        // TODO: Open a TCP socket to current_host_:current_port_.
        //
        // Pseudocode:
        //   socket_ = tcp_connect(current_host_, current_port_);
        //   if (!socket_.is_open()) {
        //       std::cerr << "Failed to connect to "
        //                 << current_host_ << ":" << current_port_ << "\n";
        //       return false;
        //   }
        //   return true;

        std::cout << "TODO: connect to " << current_host_ << ":" << current_port_ << "\n";
        connected_ = true;  // Stub: pretend we connected
        return true;
    }

    // -----------------------------------------------------------------------
    // disconnect() — close the current TCP connection.
    // -----------------------------------------------------------------------
    void disconnect() {
        // TODO: Close the TCP socket gracefully.
        //   socket_.close();
        connected_ = false;
    }

    // -----------------------------------------------------------------------
    // send_command() — send a command string to the Raft cluster.
    //
    // This method handles the NotLeader redirect loop:
    //   1. Build a ClientRequest with the command.
    //   2. Send it to the currently connected node.
    //   3. If the response says NotLeader, extract leader_hint, reconnect
    //      to the new leader, and retry.
    //   4. Return the final response string to the caller.
    //
    // Returns the response payload on success, or an error message string.
    // -----------------------------------------------------------------------
    std::string send_command(const std::string& command) {
        static constexpr int kMaxRedirects = 5;

        for (int attempt = 0; attempt < kMaxRedirects; ++attempt) {
            if (!connected_ && !connect()) {
                return "ERROR: could not connect to " + current_host_ + ":" +
                       std::to_string(current_port_);
            }

            // -----------------------------------------------------------------
            // TODO: Build and send the ClientRequest RPC.
            //
            // ClientRequest request;
            // request.command = command;
            //
            // std::string serialized = serializer_->serialize(request);
            // socket_.send(serialized);
            //
            // std::string raw_response = socket_.receive();
            // ClientResponse response = serializer_->deserialize<ClientResponse>(
            //     raw_response);
            // -----------------------------------------------------------------

            // -----------------------------------------------------------------
            // TODO: Handle the response.
            //
            // if (response.success) {
            //     return response.result;
            // }
            //
            // if (response.error == ErrorCode::NotLeader) {
            //     // The node we contacted is not the leader. It may include
            //     // a hint telling us where the real leader is.
            //     if (response.leader_hint.has_value()) {
            //         auto& hint = response.leader_hint.value();
            //         std::cout << "Redirecting to leader at "
            //                   << hint.host << ":" << hint.port << "\n";
            //         disconnect();
            //         current_host_ = hint.host;
            //         current_port_ = hint.port;
            //         continue;  // Retry with the new leader
            //     }
            //     return "ERROR: not leader and no leader hint available. "
            //            "The cluster may be electing a new leader — try again.";
            // }
            //
            // // Some other error.
            // return "ERROR: " + response.error_message;
            // -----------------------------------------------------------------

            // Stub: echo the command back since we have no real connection.
            return "TODO: send '" + command + "' to " + current_host_ + ":" +
                   std::to_string(current_port_);
        }

        return "ERROR: too many redirects (" + std::to_string(kMaxRedirects) +
               "). "
               "Could not find the cluster leader.";
    }

   private:
    std::string current_host_;    // Host of the node we are connected to
    std::uint16_t current_port_;  // Port of the node we are connected to
    bool connected_{false};

    // TODO: Add actual networking and serialization members:
    // std::shared_ptr<Serializer> serializer_;
    // TcpSocket socket_;  // or whatever the transport layer provides
};

}  // namespace raft

// =============================================================================
// Interactive REPL
// =============================================================================

// ---------------------------------------------------------------------------
// Run a read-eval-print loop that reads commands from stdin, sends them to
// the Raft cluster, and prints the response.
// ---------------------------------------------------------------------------
static void run_repl(raft::RaftClient& client) {
    std::string line;

    std::cout << "Connected to Raft cluster. Type 'help' for commands, "
              << "'quit' to exit.\n";

    while (true) {
        // Print prompt.
        std::cout << "> " << std::flush;

        // Read a line from stdin.
        if (!std::getline(std::cin, line)) {
            // EOF (e.g., piped input ended).
            std::cout << "\n";
            break;
        }

        // Trim leading/trailing whitespace (simple approach).
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos) {
            continue;  // Empty line — just re-prompt.
        }
        auto end = line.find_last_not_of(" \t");
        line = line.substr(start, end - start + 1);

        // Handle local commands that do not go to the cluster.
        if (line == "quit" || line == "exit") {
            std::cout << "Bye.\n";
            break;
        }

        if (line == "help") {
            std::cout << "Commands:\n"
                      << "  SET <key> <value>   Store a key-value pair\n"
                      << "  GET <key>           Retrieve the value for a key\n"
                      << "  DEL <key>           Delete a key\n"
                      << "  quit / exit         Disconnect and exit\n";
            continue;
        }

        // Validate that the command starts with a known verb.
        std::string verb;
        {
            std::istringstream iss(line);
            iss >> verb;
        }

        if (verb != "SET" && verb != "GET" && verb != "DEL") {
            std::cerr << "Unknown command: " << verb << ". Type 'help' for usage.\n";
            continue;
        }

        // Send the command to the Raft cluster and print the response.
        std::string response = client.send_command(line);
        std::cout << response << "\n";
    }
}

// =============================================================================
// main
// =============================================================================

int main(int argc, char* argv[]) {
    // -----------------------------------------------------------------------
    // 1. Parse command-line arguments
    // -----------------------------------------------------------------------
    ClientArgs args = parse_args(argc, argv);

    std::cout << "Raft client — connecting to " << args.host << ":" << args.port << "\n";

    // -----------------------------------------------------------------------
    // 2. Create the RaftClient and establish initial connection
    // -----------------------------------------------------------------------
    raft::RaftClient client(args.host, args.port);

    if (!client.connect()) {
        std::cerr << "Failed to establish initial connection. Exiting.\n";
        return 1;
    }

    // -----------------------------------------------------------------------
    // 3. Run the interactive command loop
    // -----------------------------------------------------------------------
    run_repl(client);

    // -----------------------------------------------------------------------
    // 4. Clean up — the RaftClient destructor handles disconnection
    // -----------------------------------------------------------------------
    return 0;
}
