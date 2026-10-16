#pragma once

#include <basalt/wire/lua/LuaArgs.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::server::plugins {
namespace pluginJson {
[[nodiscard]] std::string write(wire::lua::LuaArgs& lua, std::int32_t index);

void parse(wire::lua::LuaArgs& lua, std::string_view text);
} // namespace pluginJson
} // namespace basalt::server::plugins
