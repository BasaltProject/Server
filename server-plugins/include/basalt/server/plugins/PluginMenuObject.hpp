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
class LuaMenu : public std::enable_shared_from_this<LuaMenu> {
public:
    using Call = std::function<void(const lua::LuaRef& handler, PluginPlayerRef& player, std::int32_t slot)>;

    [[nodiscard]] static std::shared_ptr<LuaMenu> create(PluginMenus& menus, PluginPlayers& players, std::string title,
                                                         std::int32_t rows, Call call);

    void push(lua::LuaArgs& lua);

    void closeAll();

private:
    LuaMenu(PluginMenus& menus, PluginPlayers& players, std::string title, std::int32_t rows, Call call)
        : menus_(&menus), players_(&players), title_(std::move(title)), rows_(rows), call_(std::move(call)) {}

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
