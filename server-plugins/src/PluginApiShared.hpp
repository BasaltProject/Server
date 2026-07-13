#pragma once

#include <basalt/server/plugins/PluginApi.hpp>
#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginFormObject.hpp>
#include <basalt/server/plugins/PluginMenuObject.hpp>
#include <basalt/server/plugins/PluginMiscApi.hpp>
#include <basalt/server/plugins/PluginObject.hpp>
#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/server/plugins/PluginRecipeApi.hpp>
#include <basalt/server/plugins/PluginServiceRegistry.hpp>
#include <basalt/server/plugins/PluginStore.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/lua/LuaCall.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/Snbt.hpp>
#include <basalt/wire/plugins/PluginToml.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <utility>

namespace basalt::server::plugins::pluginApi::internal {
using wire::lua::LuaArgs;
using wire::lua::LuaRef;

[[nodiscard]] inline std::optional<std::string> stringField(LuaArgs& lua, std::int32_t index, std::string_view key) {
    lua.pushField(index, key);
    std::optional<std::string> value;
    if (lua.isString(-1)) {
        value = lua.toText(-1);
    }
    lua.pop(1);
    return value;
}

inline void pushHandleTable(LuaArgs& lua, PluginHandle handle) {
    const auto held = std::make_shared<PluginHandle>(std::move(handle));
    lua.pushTable();
    wire::lua::pushHostFunction(lua, "cancel", [held](LuaArgs& args) {
        static_cast<void>(args);
        held->remove();
        return 0;
    });
    lua.setField("cancel");
}

void installWorld(LuaArgs& lua, PluginInstance& instance, LoadedPlugin& plugin, PluginHost& host);
} // namespace basalt::server::plugins::pluginApi::internal
