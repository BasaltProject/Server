#pragma once

#include <string>
#include <string_view>

namespace basalt::server::lua {

/// What a plugin did wrong, said the way its author can act on it.
///
/// Lua puts the chunk name and line in the message itself (`arena/main.lua:12: ...`), so the work
/// here is keeping that intact and naming the plugin rather than wrapping it in a host stack trace
/// nobody can use.
struct LuaFailure {
    std::string plugin;
    std::string message;

    /// The message with the plugin in front, which is the line that goes to the console.
    [[nodiscard]] std::string report() const { return "[" + plugin + "] " + message; }

    /// Whether the plugin ran out of the instructions it was given for this turn, or was stopped.
    [[nodiscard]] bool outOfBudget() const;

    /// Whether the plugin asked for more memory than it is allowed.
    [[nodiscard]] bool outOfMemory() const;
};

namespace failureMessages {

inline constexpr std::string_view kBudget = "instruction budget exhausted";
inline constexpr std::string_view kInterrupted = "interrupted";
inline constexpr std::string_view kMemory = "not enough memory";

} // namespace failureMessages

} // namespace basalt::server::lua
