#pragma once

#include <basalt/wire/lua/LuaArgs.hpp>

namespace basalt::server::plugins {
class LoadedPlugin;
class PluginHost;

namespace pluginApi {

/// The `scoreboards` and `boss_bars` tables, and the read side of `access`.
void installMisc(wire::lua::LuaArgs& lua, LoadedPlugin& plugin, PluginHost& host);

} // namespace pluginApi
} // namespace basalt::server::plugins
