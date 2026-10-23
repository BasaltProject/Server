#pragma once

#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/wire/lua/LuaArgs.hpp>

namespace basalt::server::plugins::pluginApi {
/// The `recipes` table: add, remove, look up and list the server's crafting, cooking, stonecutting and smithing
/// recipes.
void installRecipes(wire::lua::LuaArgs& lua, PluginHost& host);
} // namespace basalt::server::plugins::pluginApi
