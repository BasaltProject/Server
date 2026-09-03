#pragma once

#include <basalt/server/lua/LuaRef.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginValues.hpp>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::plugins {

/// What one plugin offers another, and the only way across.
///
/// A service looks like a table of functions and is not one: a Lua table belongs to the state that
/// made it. What crosses is a call the host mediates, so the provider's function runs in the
/// provider's state and on its own budget. Values are copied, which is why they are numbers,
/// strings, booleans and tables of those; a player crosses as a uuid and is looked up on the other
/// side.
class PluginServiceRegistry {
public:
    /// What is on offer, for `/plugins` and for a plugin asking what is there.
    [[nodiscard]] std::set<std::string> names() const;

    /// Takes the table on top of the stack apart into the functions it offers.
    PluginHandle provide(lua::LuaArgs& lua, std::int32_t index, const std::string& pluginId, const std::string& name);

    /// Pushes the table a consumer gets: one host function per function the provider offered.
    ///
    /// Pushes nil when nobody offers it, which is what lets `if bank then` be the whole of a soft
    /// dependency.
    void consume(lua::LuaArgs& lua, PluginHost& host, const std::string& name);

private:
    struct Offer {
        std::string pluginId;
        /// One reference per function, in the provider's own state.
        std::map<std::string, std::shared_ptr<lua::LuaRef>, std::less<>> functions;
    };

    std::map<std::string, Offer, std::less<>> offers_;
};

/// One call across the boundary between two plugins.
///
/// It runs in the provider's state, on the provider's budget, and what goes in and comes out are
/// copies. A failure there is the provider's strike and an ordinary error for whoever called: a
/// plugin should not be taken down by a service it merely used.
[[nodiscard]] bool callService(PluginHost& host, LoadedPlugin& provider, const lua::LuaRef& function,
                               const std::vector<PluginCrossValue>& arguments, lua::LuaArgs& caller);

/// Pushes a value that crossed into whichever state is asking for it.
void pushCrossed(lua::LuaArgs& lua, const PluginCrossValue& value, PluginHost& host, LoadedPlugin& plugin);

} // namespace basalt::server::plugins
