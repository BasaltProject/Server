#pragma once

#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/wire/lua/LuaArgs.hpp>

#include <string>

namespace basalt::server::plugins {
inline constexpr std::string_view kServerScope = "server";

void pushStore(wire::lua::LuaArgs& lua, PluginHost& host, const std::string& pluginId);

void pushData(wire::lua::LuaArgs& lua, const std::string& folder);

[[nodiscard]] std::string jsonPathOf(const std::string& folder, const std::string& name);
} // namespace basalt::server::plugins
