/**
 * @file file_persistent_state.cpp
 * @brief File-backed persistent state (currentTerm + votedFor).
 *
 * Storage format:
 *   A single JSON file: state.json
 *   {
 *       "current_term": 42,
 *       "voted_for": 3          // or null if no vote
 *   }
 *
 * Atomic updates:
 *   - Write to state.json.tmp, fsync, rename to state.json.
 *   - On startup, if state.json.tmp exists without state.json, use the tmp
 *     file (the rename didn't complete, but the data was fsync'd).
 */

#include <filesystem>
#include <fstream>

#include "raft/storage/persistent_state.h"

namespace raft {

class FilePersistentState final : public PersistentState {
   public:
    explicit FilePersistentState(std::string directory) : directory_{std::move(directory)} {
        // TODO: Create the directory if it doesn't exist.
        // TODO: Recovery logic:
        //   1. If state.json exists, parse it → term_, voted_for_.
        //   2. Else if state.json.tmp exists (incomplete rename), rename it
        //      to state.json, then parse.
        //   3. Else: fresh node → term_ = Term{0}, voted_for_ = nullopt.
    }

    Term current_term() const override { return term_; }

    std::optional<NodeId> voted_for() const override { return voted_for_; }

    Result<void> save(Term term, std::optional<NodeId> voted_for) override {
        // TODO: Persist atomically.
        //
        //   1. Build JSON: { "current_term": term.value, "voted_for": ... }
        //   2. Write to state.json.tmp.
        //   3. fsync the file descriptor.
        //   4. Rename state.json.tmp → state.json.
        //   5. fsync the directory fd (ensures rename is durable on Linux).
        //   6. Update in-memory cache: term_ = term, voted_for_ = voted_for.
        //
        // CRITICAL: this must complete before the caller sends any RPC reply.
        // A failure here means we cannot guarantee safety.  Return IoError
        // and let the caller decide (likely shut down).
        return Result<void>::failure(ErrorCode::NotImplemented);
    }

   private:
    std::string directory_;
    Term term_{};
    std::optional<NodeId> voted_for_;
};

std::unique_ptr<PersistentState> create_file_persistent_state(const std::string& directory) {
    return std::make_unique<FilePersistentState>(directory);
}

}  // namespace raft
