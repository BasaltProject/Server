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

using PluginCrossTable = std::map<std::string, std::shared_ptr<PluginCrossValue>, std::less<>>;

struct PluginCrossValue {
    std::variant<std::monostate, bool, std::int64_t, double, std::string, PluginCrossTable> value;
};

inline constexpr std::string_view kPlayerMark = "__player_uuid";

inline constexpr std::int32_t kMaxCrossDepth = 8;

[[nodiscard]] PluginCrossValue crossOut(lua::LuaArgs& lua, std::int32_t index, std::int32_t depth = 0);

[[nodiscard]] std::vector<PluginCrossValue> crossArguments(lua::LuaArgs& lua);
} // namespace basalt::server::plugins
