#pragma once

#include <basalt/server/lua/LuaArgs.hpp>
#include <basalt/server/plugins/PluginHost.hpp>

#include <string>

namespace basalt::server::plugins {

/// The uuid a plugin's own data is kept under: not a player, and no player can have it.
inline constexpr std::string_view kServerScope = "server";

/// `store`: what the plugin keeps that is not about one player, and the queries over what is.
///
/// `store.record = ...` is the plugin's own key-value, kept under a scope no player can have so it
/// lives in the same table with the same isolation; `store.top("kills", 10)` is the query that made
/// this worth being a database rather than a file.
void pushStore(lua::LuaArgs& lua, PluginHost& host, const std::string& pluginId);

/// `data.json("arenas")`: a table kept in a file of the plugin's own folder.
///
/// For what is not about a player. It is read when asked for and written by `data.save`, so a crash
/// leaves the old file rather than half of a new one.
void pushData(lua::LuaArgs& lua, const std::string& folder);

/// `arenas` is `data/arenas.json` unless the plugin gave a path of its own.
[[nodiscard]] std::string jsonPathOf(const std::string& folder, const std::string& name);

} // namespace basalt::server::plugins
