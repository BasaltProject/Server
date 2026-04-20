#pragma once

#include <basalt/server/lua/LuaArgs.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

namespace basalt::server::plugins {
namespace pluginToml {
void push(lua::LuaArgs& lua, const wire::common::toml::TomlTable& table);

void pushValue(lua::LuaArgs& lua, const wire::common::toml::TomlValue& value);
} // namespace pluginToml
} // namespace basalt::server::plugins
