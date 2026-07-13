#pragma once

#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginMiscApi.hpp>
#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/wire/lua/LuaCall.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace basalt::server::plugins::pluginApi::misc {
using wire::lua::LuaArgs;

constexpr std::array<std::string_view, 16> kTeamColors{
    "black",     "dark_blue", "dark_green", "dark_aqua", "dark_red", "dark_purple",  "gold",   "gray",
    "dark_gray", "blue",      "green",      "aqua",      "red",      "light_purple", "yellow", "white"};

/// Plugin text goes to the server as MiniMessage, which the server reads when the text starts with "mini:".
[[nodiscard]] inline std::string textAt(LoadedPlugin& plugin, LuaArgs& args, std::int32_t index) {
    return "mini:" + plugin.text.resolve(args.checkString(index), &args, index + 1);
}

[[nodiscard]] inline std::int32_t slotOf(LuaArgs& args, std::int32_t index) {
    if (args.isNumber(index)) {
        return static_cast<std::int32_t>(args.toInteger(index));
    }
    const std::string name = args.checkString(index);
    if (name == "list") {
        return 0;
    }
    if (name == "sidebar") {
        return 1;
    }
    if (name == "below_name") {
        return 2;
    }
    constexpr std::string_view kTeamPrefix = "sidebar.team.";
    if (name.starts_with(kTeamPrefix)) {
        for (std::size_t color = 0; color < kTeamColors.size(); ++color) {
            if (kTeamColors[color] == std::string_view{name}.substr(kTeamPrefix.size())) {
                return static_cast<std::int32_t>(color) + 3;
            }
        }
    }
    args.raise("'" + name + "' is not a display slot: list, sidebar, below_name or sidebar.team.<color>");
}

[[nodiscard]] inline std::optional<std::string> stringField(LuaArgs& lua, std::int32_t index, std::string_view key) {
    lua.pushField(index, key);
    std::optional<std::string> value;
    if (lua.isString(-1)) {
        value = lua.toText(-1);
    }
    lua.pop(1);
    return value;
}

[[nodiscard]] inline std::optional<bool> flagField(LuaArgs& lua, std::int32_t index, std::string_view key) {
    lua.pushField(index, key);
    std::optional<bool> value;
    if (lua.isBoolean(-1)) {
        value = lua.toBoolean(-1);
    }
    lua.pop(1);
    return value;
}

[[nodiscard]] inline std::optional<double> numberField(LuaArgs& lua, std::int32_t index, std::string_view key) {
    lua.pushField(index, key);
    std::optional<double> value;
    if (lua.isNumber(-1)) {
        value = lua.toNumber(-1);
    }
    lua.pop(1);
    return value;
}

inline void pushNames(LuaArgs& args, const std::vector<std::string>& names) {
    args.pushTable();
    std::int64_t position = 1;
    for (const std::string& name : names) {
        args.pushString(name);
        args.setIndex(position++);
    }
}

[[nodiscard]] inline std::vector<std::string> namesField(LuaArgs& lua, std::int32_t index, std::string_view key) {
    std::vector<std::string> out;
    lua.pushField(index, key);
    const std::int32_t at = lua.top();
    if (lua.isString(at)) {
        out.push_back(lua.toText(at));
    } else if (lua.isTable(at)) {
        const std::int64_t count = lua.length(at);
        for (std::int64_t position = 1; position <= count; ++position) {
            lua.pushIndex(at, position);
            out.push_back(lua.toText(-1));
            lua.pop(1);
        }
    }
    lua.pop(1);
    return out;
}

inline void pushSnbt(LuaArgs& args, const std::optional<std::vector<std::uint8_t>>& bytes) {
    if (bytes.has_value()) {
        const std::expected<wire::nbt::AnonymousTag, wire::nbt::NbtError> read =
            wire::nbt::nbtIO::readAnonymous(*bytes);
        if (read.has_value()) {
            args.pushString(wire::nbt::snbt::stringify(read->tag));
            return;
        }
    }
    args.pushNil();
}

[[nodiscard]] inline std::optional<std::vector<std::uint8_t>> snbtBytes(LuaArgs& args, std::int32_t index) {
    const std::expected<wire::nbt::NbtTag, wire::nbt::NbtError> parsed =
        wire::nbt::snbt::parse(args.checkString(index));
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    return wire::nbt::nbtIO::writeAnonymous(*parsed);
}

void installServerTables(wire::lua::LuaArgs& lua, PluginHost& host);
} // namespace basalt::server::plugins::pluginApi::misc
