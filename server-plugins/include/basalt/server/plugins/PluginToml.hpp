#pragma once

#include <basalt/wire/common/toml/Toml.hpp>
#include <basalt/wire/lua/LuaArgs.hpp>

namespace basalt::server::plugins {
namespace pluginToml {
void push(wire::lua::LuaArgs& lua, const wire::common::toml::TomlTable& table);

void pushValue(wire::lua::LuaArgs& lua, const wire::common::toml::TomlValue& value);
} // namespace pluginToml
} // namespace basalt::server::plugins
