#pragma once

#include <basalt/wire/lua/LuaArgs.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::server::plugins {
using PluginValue = std::variant<std::monostate, bool, std::int64_t, double, std::string, std::vector<std::string>>;

class PluginObject {
public:
    using Reader = std::function<PluginValue()>;
    using Pusher = std::function<void(wire::lua::LuaArgs&)>;
    using Writer = std::function<void(wire::lua::LuaArgs&, std::int32_t index)>;
    using Method = std::function<std::int32_t(wire::lua::LuaArgs&)>;

    explicit PluginObject(std::string kind) : kind_(std::move(kind)) {}

    PluginObject& reads(std::string name, Reader reader);

    PluginObject& pushes(std::string name, Pusher pusher);

    PluginObject& writes(std::string name, Writer writer);

    PluginObject& method(std::string name, Method body);

    void push(wire::lua::LuaArgs& lua) &&;

private:
    std::string kind_;
    std::map<std::string, Reader, std::less<>> readers_;
    std::map<std::string, Pusher, std::less<>> pushers_;
    std::map<std::string, Writer, std::less<>> writers_;
    std::map<std::string, Method, std::less<>> methods_;
};

void pushValue(wire::lua::LuaArgs& lua, const PluginValue& value);

[[nodiscard]] std::int32_t firstArgument(const wire::lua::LuaArgs& lua);
} // namespace basalt::server::plugins
