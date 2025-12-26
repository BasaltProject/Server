#pragma once

#include <basalt/server/lua/LuaRef.hpp>
#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginHost.hpp>

#include <map>
#include <string>

namespace basalt::server::plugins {

class PluginEventView;

/// Registers a plugin's commands with the server's own dispatcher, and runs them when called.
class PluginCommandDispatch {
public:
    explicit PluginCommandDispatch(PluginHost& host) : host_(&host) {}

    /// Registers one command once, and remembers which function each lane answers it with.
    void registerCommand(LoadedPlugin& plugin, PluginInstance& instance, const PluginCommandSpec& spec,
                         lua::LuaRef handler);

private:
    /// Which lane answers this one. They take turns, so a plugin that assumed one state sees it.
    [[nodiscard]] static PluginInstance* laneFor(LoadedPlugin& plugin, CommandEntry& entry);

    static void run(PluginHost& host, LoadedPlugin& plugin, CommandEntry& entry, PluginCommandCall& call);

    PluginHost* host_;
};

/// Registers a plugin's event handlers and fires them.
class PluginEventDispatch {
public:
    explicit PluginEventDispatch(PluginHost& host) : host_(&host) {}

    /// Registers one event handler of one plugin, for one lane.
    ///
    /// The handler is the lane's, not the plugin's: with more than one lane a plugin has one state
    /// per lane and each has its own function, and the fire picks a lane the way a command does.
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
