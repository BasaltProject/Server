#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginValues.hpp>
#include <basalt/wire/lua/LuaRef.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::plugins {
class LoadedPlugin;
class PluginHost;

class PluginServiceRegistry {
public:
    [[nodiscard]] std::set<std::string> names() const;

    PluginHandle provide(wire::lua::LuaArgs& lua, std::int32_t index, const std::string& pluginId,
                         const std::string& name);

    void consume(wire::lua::LuaArgs& lua, PluginHost& host, const std::string& name);

private:
    struct Offer {
        std::string pluginId;
        std::map<std::string, std::shared_ptr<wire::lua::LuaRef>, std::less<>> functions;
    };

    std::map<std::string, Offer, std::less<>> offers_;
};

[[nodiscard]] bool callService(PluginHost& host, LoadedPlugin& provider, const wire::lua::LuaRef& function,
                               const std::vector<PluginCrossValue>& arguments, wire::lua::LuaArgs& caller);

void pushCrossed(wire::lua::LuaArgs& lua, const PluginCrossValue& value, PluginHost& host, LoadedPlugin& plugin);
} // namespace basalt::server::plugins
