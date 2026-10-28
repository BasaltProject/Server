#pragma once

#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/world/portal/PortalShape.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class AdvancementTriggers;
class LoadedWorld;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::command {
class CommandEnvironment;
}

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class ChunkSimulationView;
class NaturalSpawningSystem;

class EnvironmentTickSystem {
public:
    struct Hooks {
        std::function<bool(const player::PlayerSession& player, const world::LoadedWorld& from,
                           const world::LoadedWorld& to, double x, double y, double z, std::string_view kind)>
            portalAllowed;
        std::function<bool(const world::LoadedWorld& world, bool raining, bool thundering, bool wasRaining,
                           bool wasThundering)>
            weatherChangeAllowed;
        std::function<bool(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z)>
            lightningAllowed;
        std::function<std::optional<std::int32_t>(const player::PlayerSession& player, std::int32_t now,
                                                  std::int32_t before)>
            foodChanged;
        std::function<void(player::PlayerSession& player)> armorWatch;
        std::function<std::optional<float>(const player::PlayerSession& player, float amount, std::string_view cause,
                                           bool fast)>
            healed;
    };

    /// The storm's own stream, which is not the level's: a strike must not shift what else spawns.
    struct Options {
        std::function<std::int64_t()> nextSeed;
        world::AdvancementTriggers* advancementTriggers = nullptr;
        std::function<std::string()> difficulty;
        /// The world and spot a player respawns at when they have set one; nothing when the world spawn applies.
        std::function<std::optional<std::pair<std::string, std::array<double, 3>>>(const player::PlayerSession&)>
            respawnPoint;
    };

    using Worlds = std::function<std::vector<world::LoadedWorld*>()>;

    EnvironmentTickSystem(player::PlayerRegistry& players, const config::WorldsConfig& worldsConfig, Worlds worlds,
                          ChunkSimulationView& chunkSimulation, BlockChangeBroadcaster& broadcaster,
                          const command::CommandEnvironment& environment, NaturalSpawningSystem& spawning, Hooks hooks,
                          Options options, wire::common::Logger logger = wire::common::Logger{"World"});

    void tickPlayers(world::LoadedWorld& world);

    void tickSleep(world::LoadedWorld& world);

    void tickBorderDamage(world::LoadedWorld& world);

    /// Draws the border for whoever cannot draw it themselves. parity: nothing in the jar, which
    void tickBorderWall(world::LoadedWorld& world);

    void tickPortals(world::LoadedWorld& world);

    /// parity: `Entity.handlePortal` for everything that is not a player.
    void tickEntityPortals(world::LoadedWorld& world);

    void tickInsideBlocks(world::LoadedWorld& world);

    void honeySlide(world::LoadedWorld& world, player::PlayerSession& player, std::int32_t x, std::int32_t y,
                    std::int32_t z, std::int32_t state);

    void tickLocation(world::LoadedWorld& world);

    [[nodiscard]] std::optional<std::array<double, 3>> enterGateway(world::LoadedWorld& world,
                                                                    const std::array<double, 6>& box);

    /// parity: `TheEndGatewayBlockEntity.triggerCooldown`; false while it is still cooling down.
    [[nodiscard]] bool triggerGatewayCooldown(const world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                                              std::int32_t z);

    void tickSubmersion(world::LoadedWorld& world);

    void tickTime(world::LoadedWorld& world);

private:
    [[nodiscard]] world::LoadedWorld* counterpart(const world::LoadedWorld& world, config::WorldDimension dimension);

    [[nodiscard]] world::LoadedWorld* netherCounterpart(const world::LoadedWorld& world);

    [[nodiscard]] world::LoadedWorld* endCounterpart(const world::LoadedWorld& world);

    void travelThroughPortal(world::LoadedWorld& world, player::PlayerSession& player,
                             server::world::portal::PortalAxis axis);

    void travelThroughEndPortal(world::LoadedWorld& world, player::PlayerSession& player);

    void travelThroughGateway(world::LoadedWorld& world, player::PlayerSession& player,
                              const std::array<std::int32_t, 3>& gateway);

    [[nodiscard]] std::optional<std::array<std::int32_t, 3>> touchGateway(world::LoadedWorld& world,
                                                                          const std::array<double, 6>& box);

    void tickGatewayCooldowns(const world::LoadedWorld& world);

    void strikeLightning(world::LoadedWorld& world);

    player::PlayerRegistry* players_;
    const config::WorldsConfig* worldsConfig_;
    Worlds worlds_;
    ChunkSimulationView* chunkSimulation_;
    const command::CommandEnvironment* environment_;
    NaturalSpawningSystem* spawning_;
    Hooks hooks_;
    Options options_;
    wire::common::Logger logger_;
    std::set<std::pair<std::string, config::WorldDimension>> warnedMissingDimensions_;
    std::map<std::pair<std::string, std::array<std::int32_t, 3>>, std::int32_t> gatewayCooldowns_;
    std::mutex gatewayLock_;
};

} // namespace basalt::server::runtime
