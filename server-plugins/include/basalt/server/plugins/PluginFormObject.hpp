#pragma once

#include <basalt/server/plugins/PluginServices.hpp>
#include <basalt/wire/lua/LuaArgs.hpp>
#include <basalt/wire/lua/LuaRef.hpp>

#include <map>
#include <memory>
#include <string>

namespace basalt::server::plugins {
class LuaForm : public std::enable_shared_from_this<LuaForm> {
public:
    using Call = std::function<void(const wire::lua::LuaRef& handler, PluginPlayerRef& player, const PluginForm& form,
                                    std::size_t button, const std::map<std::string, std::string>& values)>;

    [[nodiscard]] static std::shared_ptr<LuaForm> create(PluginMenus& menus, PluginPlayers& players, PluginForm form,
                                                         Call call);

    void push(wire::lua::LuaArgs& lua);

    void closeAll();

private:
    LuaForm(PluginMenus& menus, PluginPlayers& players, PluginForm form, Call call)
        : menus_(&menus), players_(&players), form_(std::move(form)), call_(std::move(call)) {}

    PluginMenus* menus_;
    PluginPlayers* players_;
    PluginForm form_;
    Call call_;
    std::map<std::string, PluginHandle> opened_;
    std::map<std::string, std::shared_ptr<wire::lua::LuaRef>> handlers_;
};

/// The form `form{ ... }` describes, read from the table at `index`; raises on a shape it cannot use.
[[nodiscard]] PluginForm readForm(wire::lua::LuaArgs& lua, std::int32_t index,
                                  const std::function<std::string(const std::string&)>& resolve);
} // namespace basalt::server::plugins
