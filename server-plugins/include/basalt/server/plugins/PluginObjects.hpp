#pragma once

#include <basalt/server/lua/LuaArgs.hpp>
#include <basalt/server/lua/LuaRef.hpp>
#include <basalt/server/plugins/PluginObject.hpp>
#include <basalt/server/plugins/PluginServices.hpp>
#include <basalt/server/plugins/PluginText.hpp>
#include <basalt/server/plugins/PluginWorldAccess.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace basalt::server::plugins {
inline constexpr std::int32_t kHotbarStart = 27;

struct PluginPoint {
    std::optional<std::string> world;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

[[nodiscard]] std::optional<PluginPoint> pointFrom(lua::LuaArgs& lua, std::int32_t index);

[[nodiscard]] std::optional<PluginItem> itemFrom(lua::LuaArgs& lua, std::int32_t index);

[[nodiscard]] std::shared_ptr<PluginPlayerRef> playerFrom(lua::LuaArgs& lua, std::int32_t index,
                                                          PluginPlayers& players);

[[nodiscard]] std::string resolved(const PluginText* text, lua::LuaArgs& lua);

void pushPosition(lua::LuaArgs& lua, std::string world, double x, double y, double z);

void pushItem(lua::LuaArgs& lua, const PluginItem& item);

void pushInventory(lua::LuaArgs& lua, const std::shared_ptr<PluginPlayerRef>& owner);

void pushEntity(lua::LuaArgs& lua, std::shared_ptr<PluginEntityRef> entity);

void pushPlayer(lua::LuaArgs& lua, std::shared_ptr<PluginPlayerRef> player, PluginStorage* storage = nullptr,
                const std::string& pluginId = {}, const PluginText* text = nullptr);

void pushPlayerData(lua::LuaArgs& lua, PluginStorage& storage, std::string pluginId, std::string uuid);

struct WorldHooks {
    using NpcClick = std::function<void(const lua::LuaRef& handler, const std::string& playerUuid)>;

    NpcClick onNpcClick;
    std::function<std::string(const std::string& text)> resolveText;
};

void pushNpc(lua::LuaArgs& lua, std::shared_ptr<PluginNpcRef> npc);

void pushHologram(lua::LuaArgs& lua, std::shared_ptr<PluginHologramRef> hologram);

void pushWorld(lua::LuaArgs& lua, std::shared_ptr<PluginWorldRef> world, WorldHooks hooks = {});
} // namespace basalt::server::plugins
