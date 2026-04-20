#pragma once

#include <basalt/server/lua/LuaArgs.hpp>
#include <basalt/server/plugins/PluginHost.hpp>

#include <string>

namespace basalt::server::plugins {
inline constexpr std::string_view kServerScope = "server";

void pushStore(lua::LuaArgs& lua, PluginHost& host, const std::string& pluginId);

void pushData(lua::LuaArgs& lua, const std::string& folder);

[[nodiscard]] std::string jsonPathOf(const std::string& folder, const std::string& name);
} // namespace basalt::server::plugins
