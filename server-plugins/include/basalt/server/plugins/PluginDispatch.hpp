#pragma once

#include <basalt/server/lua/LuaRef.hpp>
#include <basalt/server/plugins/PluginCommandSpec.hpp>
#include <basalt/server/plugins/PluginEvents.hpp>

#include <map>
#include <string>

namespace basalt::server::plugins {
struct CommandEntry;
class LoadedPlugin;
class PluginCommandCall;
class PluginEventView;
class PluginHost;
struct PluginInstance;

class PluginCommandDispatch {
public:
    explicit PluginCommandDispatch(PluginHost& host) : host_(&host) {}

    void registerCommand(LoadedPlugin& plugin, PluginInstance& instance, const PluginCommandSpec& spec,
                         lua::LuaRef handler);

private:
    [[nodiscard]] static PluginInstance* laneFor(LoadedPlugin& plugin, CommandEntry& entry);

    static void run(PluginHost& host, LoadedPlugin& plugin, CommandEntry& entry, PluginCommandCall& call);

    PluginHost* host_;
};

class PluginEventDispatch {
public:
    explicit PluginEventDispatch(PluginHost& host) : host_(&host) {}

    void registerEventHandler(LoadedPlugin& plugin, PluginInstance& instance, const PluginEventType& type,
                              PluginEventPriority priority, bool observer, bool wantsCancelled,
                              std::map<std::string, PluginEventValue, std::less<>> filters, lua::LuaRef handler);

private:
    void dispatch(PluginHost& host, LoadedPlugin& plugin, PluginInstance& instance, const lua::LuaRef& handler,
                  PluginEventView& view, bool observer, const std::string& typeName);

    PluginHost* host_;
    bool warnedAboutThread_ = false;
};
} // namespace basalt::server::plugins
