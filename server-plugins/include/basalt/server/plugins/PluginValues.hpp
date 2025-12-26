#pragma once

#include <basalt/server/lua/LuaArgs.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::server::plugins {

struct PluginCrossValue;

/// A table that crossed, by key. Held by pointer so the value type can nest without being infinite.
using PluginCrossTable = std::map<std::string, std::shared_ptr<PluginCrossValue>, std::less<>>;

/// Values that may cross between two plugins: copies, never the thing itself.
///
/// A player crosses as their uuid and is looked up on the other side, so what the provider holds is
/// its own handle rather than one made in somebody else's state.
struct PluginCrossValue {
    std::variant<std::monostate, bool, std::int64_t, double, std::string, PluginCrossTable> value;
};

/// The key a player is recognised by once it has crossed: a table with nothing else in it.
inline constexpr std::string_view kPlayerMark = "__player_uuid";

/// How deep a table may nest before the copy gives up, which is what stops a cycle.
inline constexpr std::int32_t kMaxCrossDepth = 8;

/// Reads one value out of a state, as a copy.
[[nodiscard]] PluginCrossValue crossOut(lua::LuaArgs& lua, std::int32_t index, std::int32_t depth = 0);

/// Every argument of a call, in order.
[[nodiscard]] std::vector<PluginCrossValue> crossArguments(lua::LuaArgs& lua);

} // namespace basalt::server::plugins
