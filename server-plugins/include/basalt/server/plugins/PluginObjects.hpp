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

/// Vanilla's own numbering, which is what an event carries and what a click indexes.
inline constexpr std::int32_t kHotbarStart = 27;

/// A world and three numbers, however the plugin spelled them.
struct PluginPoint {
    std::optional<std::string> world;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/// Reads a position out of a table that has `x`, `y` and `z`: our own or the plugin's own.
[[nodiscard]] std::optional<PluginPoint> pointFrom(lua::LuaArgs& lua, std::int32_t index);

/// What a plugin wrote where a stack was expected.
///
/// `"minecraft:bread"` and `{ id = "minecraft:bread", count = 3 }` are both one, because asking for a
/// table to hand over a single item is the kind of ceremony this API exists to avoid.
[[nodiscard]] std::optional<PluginItem> itemFrom(lua::LuaArgs& lua, std::int32_t index);

/// The player behind a Lua player object, for a call that takes one.
[[nodiscard]] std::shared_ptr<PluginPlayerRef> playerFrom(lua::LuaArgs& lua, std::int32_t index,
                                                          PluginPlayers& players);

/// The message the plugin wrote, with its `@key` looked up and its `{name}` filled in.
[[nodiscard]] std::string resolved(const PluginText* text, lua::LuaArgs& lua);

void pushPosition(lua::LuaArgs& lua, std::string world, double x, double y, double z);

/// One stack in Lua. Read-only, because a stack is a value: to change it, put another one in the
/// slot.
void pushItem(lua::LuaArgs& lua, const PluginItem& item);

/// The slots, as `inv:get(0)` and `inv:set(0, item)`.
void pushInventory(lua::LuaArgs& lua, const std::shared_ptr<PluginPlayerRef>& owner);

void pushEntity(lua::LuaArgs& lua, std::shared_ptr<PluginEntityRef> entity);

/// Turns one player into the object a plugin holds.
///
/// `storage` and `pluginId` give it its `data` table, which is namespaced by the plugin; without
/// them the field answers nil, which is what a call that has no plugin behind it gets.
void pushPlayer(lua::LuaArgs& lua, std::shared_ptr<PluginPlayerRef> player, PluginStorage* storage = nullptr,
                const std::string& pluginId = {}, const PluginText* text = nullptr);

/// `p.data.kills = (p.data.kills or 0) + 1`.
///
/// A table whose reads and writes go to the player database, in this plugin's namespace: it looks
/// like a field of the player because that is what it is, and it survives them logging out.
void pushPlayerData(lua::LuaArgs& lua, PluginStorage& storage, std::string pluginId, std::string uuid);

/// What a world's own objects need from above it: how a click on an NPC gets back into the plugin's
/// state, and how text put in the world has its `@key` and `{name}` filled in.
struct WorldHooks {
    using NpcClick = std::function<void(const lua::LuaRef& handler, const std::string& playerUuid)>;

    NpcClick onNpcClick;
    std::function<std::string(const std::string& text)> resolveText;
};

/// A player-shaped entity: the same handle a hologram has, plus a name, a skin and a look.
void pushNpc(lua::LuaArgs& lua, std::shared_ptr<PluginNpcRef> npc);

/// Floating text: an entity object with its text and who it is for.
void pushHologram(lua::LuaArgs& lua, std::shared_ptr<PluginHologramRef> hologram);

void pushWorld(lua::LuaArgs& lua, std::shared_ptr<PluginWorldRef> world, WorldHooks hooks = {});

} // namespace basalt::server::plugins
