#pragma once

#include <basalt/server/lua/LuaArgs.hpp>
#include <basalt/server/lua/LuaRef.hpp>
#include <basalt/server/plugins/PluginServices.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace basalt::server::plugins {

/// A window, and what happens when somebody clicks it.
///
/// The slot numbers are the menu's own, 0 upwards, which is the one place this API deliberately does
/// not use vanilla's numbering: a menu is not a container, its rows are its own, and making a plugin
/// add 9 to reach the second row of its own window is exactly the arithmetic this removes.
class LuaMenu : public std::enable_shared_from_this<LuaMenu> {
public:
    /// Runs a plugin's click handler on its own state, mediated the way every callback here is.
    using Call = std::function<void(const lua::LuaRef& handler, PluginPlayerRef& player, std::int32_t slot)>;

    [[nodiscard]] static std::shared_ptr<LuaMenu> create(PluginMenus& menus, PluginPlayers& players, std::string title,
                                                         std::int32_t rows, Call call);

    /// Pushes the object a plugin holds. Keeps this menu alive for as long as the object is.
    void push(lua::LuaArgs& lua);

    /// Shuts every copy of this menu: what unloading the plugin has to do.
    void closeAll();

private:
    LuaMenu(PluginMenus& menus, PluginPlayers& players, std::string title, std::int32_t rows, Call call)
        : menus_(&menus), players_(&players), title_(std::move(title)), rows_(rows), call_(std::move(call)) {}

    /// Everyone looking at this menu sees the change, without the plugin reopening it.
    void redraw();

    PluginMenus* menus_;
    PluginPlayers* players_;
    std::string title_;
    std::int32_t rows_;
    Call call_;
    std::map<std::int32_t, PluginItem> items_;
    std::map<std::int32_t, lua::LuaRef> handlers_;
    std::map<std::string, PluginHandle> opened_;
};

} // namespace basalt::server::plugins
