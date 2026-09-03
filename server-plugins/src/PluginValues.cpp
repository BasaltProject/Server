#include <basalt/server/plugins/PluginValues.hpp>

#include <utility>

namespace basalt::server::plugins {
namespace {

using lua::LuaArgs;

/// One string field of the table at `index`, or nothing when it is absent.
[[nodiscard]] std::optional<std::string> stringField(LuaArgs& lua, std::int32_t index, std::string_view key) {
    lua.pushField(index, key);
    std::optional<std::string> value;
    if (lua.isString(-1)) {
        value = lua.toText(-1);
    }
    lua.pop(1);
    return value;
}

} // namespace

PluginCrossValue crossOut(LuaArgs& lua, std::int32_t index, std::int32_t depth) {
    if (index > lua.count() && index > 0) {
        return {};
    }
    if (lua.isNil(index)) {
        return {};
    }
    if (lua.isBoolean(index)) {
        return PluginCrossValue{.value = lua.toBoolean(index)};
    }
    if (lua.isNumber(index)) {
        return lua.isInteger(index) ? PluginCrossValue{.value = lua.toInteger(index)}
                                    : PluginCrossValue{.value = lua.toNumber(index)};
    }
    if (lua.isString(index)) {
        return PluginCrossValue{.value = lua.toText(index)};
    }
    if (!lua.isTable(index) || depth >= kMaxCrossDepth) {
        return {};
    }
    // A player object is recognised by what it answers, not by its identity: it is a table with a
    // uuid, and that uuid is all the other side needs.
    const std::optional<std::string> uuid = stringField(lua, index, "uuid");
    const std::optional<std::string> name = stringField(lua, index, "name");
    PluginCrossTable table;
    if (uuid.has_value() && name.has_value()) {
        table.emplace(std::string{kPlayerMark}, std::make_shared<PluginCrossValue>(PluginCrossValue{.value = *uuid}));
        return PluginCrossValue{.value = std::move(table)};
    }
    lua.pushNil();
    while (lua.nextEntry(index)) {
        table.emplace(lua.toText(-2), std::make_shared<PluginCrossValue>(crossOut(lua, lua.top(), depth + 1)));
        lua.pop(1);
    }
    return PluginCrossValue{.value = std::move(table)};
}

std::vector<PluginCrossValue> crossArguments(LuaArgs& lua) {
    std::vector<PluginCrossValue> copied;
    const std::int32_t count = lua.count();
    copied.reserve(static_cast<std::size_t>(count));
    for (std::int32_t index = 1; index <= count; index++) {
        copied.push_back(crossOut(lua, index));
    }
    return copied;
}

} // namespace basalt::server::plugins
