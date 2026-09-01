#pragma once

#include <basalt/server/lua/LuaArgs.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

namespace basalt::server::plugins {

/// A plugin's configuration, as a Lua table.
///
/// TOML rather than a format of the plugin's choosing because it is what the operator edits
/// everywhere else on this server, and the numbers keep the type they were written with: an integer
/// stays an integer rather than becoming a float on the way in.
namespace pluginToml {

/// Pushes the table onto the stack, ready for the caller to name it.
void push(lua::LuaArgs& lua, const wire::common::toml::TomlTable& table);

/// Pushes one value, whatever kind it is.
void pushValue(lua::LuaArgs& lua, const wire::common::toml::TomlValue& value);

} // namespace pluginToml

} // namespace basalt::server::plugins
