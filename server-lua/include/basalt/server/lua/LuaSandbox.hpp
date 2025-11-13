#pragma once

#include <basalt/server/lua/LuaFailure.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct lua_State;

namespace basalt::server::lua {

/// What one plugin may spend before the server takes its turn back.
struct LuaLimits {
    /// Instructions per handover, not per lifetime: every entry from the server refills it, and what
    /// the plugin does inside spends from the same ceiling, coroutines included.
    ///
    /// The default is the one the frozen Kotlin measured for its own interpreter: a quarter of a
    /// million, about a quarter of a tick there. Plenty for anything a handler legitimately does in
    /// one turn, and a hard stop well inside the tick for anything that has run away.
    std::int64_t instructionsPerTurn = 250000;
    std::size_t memoryBytes = 32ULL * 1024 * 1024;
};

/// A chunk compiled once, for every lane to share.
///
/// Official Lua has no prototype object a second state can borrow, so what is shared is the dumped
/// bytecode: finished code and constants, loaded into as many states as there are lanes. The bytes
/// never come from a plugin, only from this class's own compiler, which is why loading them back is
/// not the unverified-bytecode hole that `load` in binary mode would be.
struct LuaChunk {
    std::string chunkName;
    std::vector<std::uint8_t> bytecode;
};

/// One Lua state with everything a plugin must not reach taken out of it.
///
/// There is one of these per plugin and lane, never one shared: a state is owned like everything
/// else, and two lanes running at once would corrupt a shared one.
class LuaSandbox {
public:
    /// Where `require` looks. Answering nothing is "no such module", not an error.
    using SourceFinder = std::function<std::optional<std::vector<std::uint8_t>>(std::string_view name)>;
    /// Where `print` goes. Nothing a plugin runs writes to stdout on its own.
    using PrintSink = std::function<void(std::string_view line)>;

    LuaSandbox(const LuaSandbox&) = delete;
    LuaSandbox& operator=(const LuaSandbox&) = delete;
    LuaSandbox(LuaSandbox&&) = delete;
    LuaSandbox& operator=(LuaSandbox&&) = delete;
    ~LuaSandbox();

    /// Builds a state, or answers nothing when there was not enough memory for even an empty one.
    [[nodiscard]] static std::unique_ptr<LuaSandbox> create(std::string name, LuaLimits limits = {},
                                                            std::uint64_t seed = 0, SourceFinder sources = {},
                                                            PrintSink printLine = {});

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    [[nodiscard]] const LuaLimits& limits() const noexcept { return limits_; }

    [[nodiscard]] lua_State* state() const noexcept { return state_; }

    /// Compiles source into a chunk this or any other state can bind.
    [[nodiscard]] std::expected<LuaChunk, LuaFailure> compile(std::string_view source, std::string chunkName);

    /// Loads a compiled chunk and leaves the resulting function on the stack.
    [[nodiscard]] std::expected<void, LuaFailure> bind(const LuaChunk& chunk);

    /// Loads and calls a chunk, which is what running a plugin's own file amounts to.
    [[nodiscard]] std::expected<void, LuaFailure> run(const LuaChunk& chunk);

    /// Gives the state its ceiling back. Every entry from the server is a fresh turn.
    void refill();

    /// Stops whatever is running, from a thread that is not the one running it.
    void interrupt();

    /// Starts the memory tally again, at a point where the lane is quiet.
    void resetMemoryCount();

    [[nodiscard]] std::size_t memoryUsed() const noexcept { return memoryUsed_.load(std::memory_order_relaxed); }

    [[nodiscard]] std::int64_t instructionsLeft() const noexcept { return budget_.load(std::memory_order_relaxed); }

    /// Turns whatever is on top of the stack into a failure naming this plugin, and pops it.
    [[nodiscard]] LuaFailure failureFromStack();

private:
    LuaSandbox(std::string name, LuaLimits limits, SourceFinder sources, PrintSink printLine);

    friend struct SandboxAccess;

    void install(std::uint64_t seed);
    void strip();

    std::string name_;
    LuaLimits limits_;
    SourceFinder sources_;
    PrintSink printLine_;
    lua_State* state_ = nullptr;

    /// Read and written by the count hook on the running thread and by `interrupt` from another, so
    /// it is atomic rather than plain.
    std::atomic<std::int64_t> budget_{0};
    std::atomic<bool> interrupted_{false};
    std::atomic<std::size_t> memoryUsed_{0};
    std::size_t memoryCeiling_ = 0;
};

} // namespace basalt::server::lua
