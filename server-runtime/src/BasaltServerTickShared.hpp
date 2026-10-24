#pragma once

#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/BlockTests.hpp>
#include <basalt/server/runtime/FishingSystem.hpp>
#include <basalt/server/runtime/LightningBoltSystem.hpp>
#include <basalt/server/runtime/entity/AreaEffectClouds.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/DeathDrops.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/FireworkRockets.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>
#include <basalt/server/runtime/entity/MobCombat.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/PrimedTnt.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/Shulkers.hpp>
#include <basalt/server/runtime/entity/SwimPath.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/world/AttachedEntities.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/ContainerDrops.hpp>
#include <basalt/server/runtime/world/ContainerLoot.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/DroppedItemNbt.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/EnderEyes.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/Hoppers.hpp>
#include <basalt/server/runtime/world/ItemFrames.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <random>
#include <set>
#include <string>

namespace basalt::server::runtime::basaltServerTick {

/// parity: `TntBlock.explode`, which is where the fuse is heard being lit.
constexpr std::string_view kTntPrimedSound = "entity.tnt.primed";

[[nodiscard]] inline const wire::common::Identifier& playerType() {
    static const wire::common::Identifier player = wire::common::Identifier::vanilla("player");
    return player;
}

/// parity: `random.nextDouble() * (float)(Math.PI * 2)`.
constexpr double kFullTurn = 6.283185307179586;

[[nodiscard]] inline std::int64_t scratchSeed() {
    static thread_local std::mt19937_64 random{std::random_device{}()};
    return static_cast<std::int64_t>(random());
}

[[nodiscard]] inline bool isTargetable(const player::PlayerSession& player) {
    // parity: `TargetingConditions.forCombat`, which skips whoever cannot be fought.
    return !player.dead && player.gamemode != wire::protocol::play::Gamemode::Creative &&
           player.gamemode != wire::protocol::play::Gamemode::Spectator;
}

[[nodiscard]] inline bool isSpectator(const player::PlayerSession& player) {
    return player.gamemode == wire::protocol::play::Gamemode::Spectator;
}

/// parity: `Entity.isInWater`, near enough: water where the feet are.
[[nodiscard]] inline bool feetInWater(world::LoadedWorld& world, const player::PlayerSession& player) {
    const std::optional<std::int32_t> state = world.state().blockState(
        wire::common::mth::floor(player.x), wire::common::mth::floor(player.y), wire::common::mth::floor(player.z));
    return state.has_value() && entity::swimPath::holdsWater(*state);
}

inline void announceVehicleMove(const world::LoadedWorld& world, const entity::ServerEntity& vehicle,
                                const std::array<double, 3>& from) {
    if (vehicle.removed || (vehicle.x == from[0] && vehicle.y == from[1] && vehicle.z == from[2])) {
        return;
    }
    world.events().notify(&server::plugins::pluginEventTypes::vehicleMove, [&](server::plugins::PluginEvent& event) {
        plugins::hookEvents::entity(event, world.name(), vehicle);
        event.set("from_x", from[0]);
        event.set("from_y", from[1]);
        event.set("from_z", from[2]);
    });
}

} // namespace basalt::server::runtime::basaltServerTick
