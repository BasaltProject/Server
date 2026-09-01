#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>

#include <vector>

namespace basalt::server::plugins {

/// The events this server fires, and what each one carries.
///
/// Names are dotted and read as `thing.happening`. There are far fewer of these than Bukkit has
/// classes, and on purpose: what Bukkit splits into a dozen subclasses is one event with a field
/// here (`entity.damage` with `cause`, not twelve `EntityDamageBy*Event`).
///
/// Two conventions worth knowing before reading the list. A player is carried as `player_name` and
/// `player_uuid` rather than as an object, because the object model is the next stage and the name
/// `player` is being kept free for it. And a position is `world`, `x`, `y`, `z`, in blocks.
namespace pluginEventTypes {

[[nodiscard]] const PluginEventType& playerJoin();
[[nodiscard]] const PluginEventType& playerQuit();
[[nodiscard]] const PluginEventType& playerChat();
[[nodiscard]] const PluginEventType& playerCommand();
[[nodiscard]] const PluginEventType& playerDeath();
[[nodiscard]] const PluginEventType& playerMove();
[[nodiscard]] const PluginEventType& playerTeleport();
[[nodiscard]] const PluginEventType& playerWorldChange();
[[nodiscard]] const PluginEventType& playerRespawn();
[[nodiscard]] const PluginEventType& playerInteract();
[[nodiscard]] const PluginEventType& playerInteractEntity();
[[nodiscard]] const PluginEventType& playerItemDrop();
[[nodiscard]] const PluginEventType& playerItemPickup();
[[nodiscard]] const PluginEventType& playerItemHeld();
[[nodiscard]] const PluginEventType& playerItemConsume();
[[nodiscard]] const PluginEventType& playerSwapHands();
[[nodiscard]] const PluginEventType& playerSneak();
[[nodiscard]] const PluginEventType& playerGamemodeChange();
[[nodiscard]] const PluginEventType& playerFoodChange();
[[nodiscard]] const PluginEventType& playerLevelChange();
[[nodiscard]] const PluginEventType& playerSignChange();
[[nodiscard]] const PluginEventType& playerBucket();
[[nodiscard]] const PluginEventType& blockBreak();
[[nodiscard]] const PluginEventType& blockPlace();
[[nodiscard]] const PluginEventType& entityDamage();
[[nodiscard]] const PluginEventType& entityDeath();
[[nodiscard]] const PluginEventType& serverTick();
[[nodiscard]] const PluginEventType& blockExplode();
[[nodiscard]] const PluginEventType& blockIgnite();
[[nodiscard]] const PluginEventType& blockGrow();
[[nodiscard]] const PluginEventType& blockDispense();
[[nodiscard]] const PluginEventType& blockPiston();
[[nodiscard]] const PluginEventType& worldLoad();
[[nodiscard]] const PluginEventType& worldSave();
[[nodiscard]] const PluginEventType& worldChunkLoad();
[[nodiscard]] const PluginEventType& worldChunkUnload();
[[nodiscard]] const PluginEventType& worldWeatherChange();
[[nodiscard]] const PluginEventType& worldLightning();
[[nodiscard]] const PluginEventType& worldTimeSkip();
[[nodiscard]] const PluginEventType& worldGameRuleChange();
[[nodiscard]] const PluginEventType& worldDifficultyChange();
[[nodiscard]] const PluginEventType& serverLoad();
[[nodiscard]] const PluginEventType& serverStop();
[[nodiscard]] const PluginEventType& serverPing();
[[nodiscard]] const PluginEventType& serverCommand();
[[nodiscard]] const PluginEventType& serverBroadcast();
[[nodiscard]] const PluginEventType& entitySpawn();
[[nodiscard]] const PluginEventType& entityTame();
[[nodiscard]] const PluginEventType& entityBreed();
[[nodiscard]] const PluginEventType& entityMount();
[[nodiscard]] const PluginEventType& entityHeal();
[[nodiscard]] const PluginEventType& entityShoot();
[[nodiscard]] const PluginEventType& itemSpawn();
[[nodiscard]] const PluginEventType& playerLogin();
[[nodiscard]] const PluginEventType& playerBed();
[[nodiscard]] const PluginEventType& playerEffect();
[[nodiscard]] const PluginEventType& inventoryOpen();
[[nodiscard]] const PluginEventType& inventoryClose();
[[nodiscard]] const PluginEventType& inventoryClick();
[[nodiscard]] const PluginEventType& inventorySmelt();
[[nodiscard]] const PluginEventType& inventoryMoveItem();
[[nodiscard]] const PluginEventType& inventoryTrade();
[[nodiscard]] const PluginEventType& blockDamage();
[[nodiscard]] const PluginEventType& playerItemDamage();
[[nodiscard]] const PluginEventType& playerSprint();
[[nodiscard]] const PluginEventType& playerPickItem();
[[nodiscard]] const PluginEventType& playerPortal();
[[nodiscard]] const PluginEventType& raidFinish();
[[nodiscard]] const PluginEventType& entityCombust();
[[nodiscard]] const PluginEventType& blockFade();

/// Every one of them, in the order they are declared above, which is the order `/plugins events`
/// lists them and the order the bus is filled in.
[[nodiscard]] const std::vector<const PluginEventType*>& all();

/// The two names that are not broadcast: they are about one plugin's own life.
inline constexpr std::string_view kPluginEnable = "plugin.enable";
inline constexpr std::string_view kPluginDisable = "plugin.disable";

} // namespace pluginEventTypes

} // namespace basalt::server::plugins
