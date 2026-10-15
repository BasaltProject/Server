#pragma once

#include <basalt/server/lua/LuaRef.hpp>
#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginValues.hpp>

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

    PluginHandle provide(lua::LuaArgs& lua, std::int32_t index, const std::string& pluginId, const std::string& name);

    void consume(lua::LuaArgs& lua, PluginHost& host, const std::string& name);

private:
    struct Offer {
        std::string pluginId;
        std::map<std::string, std::shared_ptr<lua::LuaRef>, std::less<>> functions;
    };

    std::map<std::string, Offer, std::less<>> offers_;
};

[[nodiscard]] bool callService(PluginHost& host, LoadedPlugin& provider, const lua::LuaRef& function,
                               const std::vector<PluginCrossValue>& arguments, lua::LuaArgs& caller);

void pushCrossed(lua::LuaArgs& lua, const PluginCrossValue& value, PluginHost& host, LoadedPlugin& plugin);
} // namespace basalt::server::plugins
