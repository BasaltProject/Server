#pragma once

#include <basalt/server/lua/LuaArgs.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::server::plugins {

/// What a field of a server object answers with: the shapes Lua has, and nothing else.
using PluginValue = std::variant<std::monostate, bool, std::int64_t, double, std::string, std::vector<std::string>>;

/// An object of the server as Lua sees it: `p.health = p.health + 1` reads and writes the player who
/// is standing there.
///
/// A handle rather than a copy, because a copy is a lie the moment anything moves. Reading a name the
/// object does not have is an error listing what it does have: a silent nil is a bug that shows up
/// three lines later.
///
/// Built once and pushed as a table with a metatable of two host functions. The readers, writers and
/// methods live as long as the state, so an object a plugin stashed keeps working.
class PluginObject {
public:
    using Reader = std::function<PluginValue()>;
    /// A reader that pushes its own value, for a field whose answer is another object.
    using Pusher = std::function<void(lua::LuaArgs&)>;
    using Writer = std::function<void(lua::LuaArgs&, std::int32_t index)>;
    using Method = std::function<std::int32_t(lua::LuaArgs&)>;

    explicit PluginObject(std::string kind) : kind_(std::move(kind)) {}

    PluginObject& reads(std::string name, Reader reader);

    /// A field whose value is pushed rather than converted: another object, or a table.
    PluginObject& pushes(std::string name, Pusher pusher);

    PluginObject& writes(std::string name, Writer writer);

    PluginObject& method(std::string name, Method body);

    /// Pushes the finished table. Everything it needs is captured, so the builder may go.
    void push(lua::LuaArgs& lua) &&;

private:
    std::string kind_;
    std::map<std::string, Reader, std::less<>> readers_;
    std::map<std::string, Pusher, std::less<>> pushers_;
    std::map<std::string, Writer, std::less<>> writers_;
    std::map<std::string, Method, std::less<>> methods_;
};

/// Pushes one value of the host's own shapes.
void pushValue(lua::LuaArgs& lua, const PluginValue& value);

/// Both `p:send(text)` and `p.send(text)` reach a method; the first passes the object as well, and a
/// plugin should not have to care which it wrote.
[[nodiscard]] std::int32_t firstArgument(const lua::LuaArgs& lua);

} // namespace basalt::server::plugins
