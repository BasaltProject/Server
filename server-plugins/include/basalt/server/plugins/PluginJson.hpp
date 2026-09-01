#pragma once

#include <basalt/server/lua/LuaArgs.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::server::plugins {

/// JSON, as much of it as a Lua table is.
///
/// Written here rather than pulled in because the mapping is the whole problem: a Lua table is an
/// array when its keys are 1..n and an object otherwise, and no general-purpose JSON library knows
/// that. Numbers keep their kind (7 stays 7, not 7.0), which is what makes a saved file readable by
/// a person.
namespace pluginJson {

/// Writes the value at `index` on the stack.
[[nodiscard]] std::string write(lua::LuaArgs& lua, std::int32_t index);

/// Reads one document and leaves the table it made on the stack. Anything that is not an object or
/// an array leaves an empty table, which is what a plugin asking for a document expects.
void parse(lua::LuaArgs& lua, std::string_view text);

} // namespace pluginJson

} // namespace basalt::server::plugins
