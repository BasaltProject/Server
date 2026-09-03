#pragma once

#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/world/portal/PortalShape.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::command {
class CommandEnvironment;
}

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class ChunkSimulationView;
class NaturalSpawningSystem;

/// Everything the world itself owes the environment around a player: sleep, the border, drowning
/// and burning, the clock, weather, lightning and the two kinds of portal.
class EnvironmentTickSystem {
public:
    /// What a plugin gets a say in. Callbacks rather than the plugin bus itself, and unset means
    /// nobody objects.
    struct Hooks {
        /// Whether this player may travel, and to where. Answers false to keep them where they are.
        std::function<bool(const player::PlayerSession& player, const world::LoadedWorld& from,
                           const world::LoadedWorld& to, double x, double y, double z, std::string_view kind)>
            portalAllowed;
        /// Whether the weather may change to this, given what it was.
        std::function<bool(const world::LoadedWorld& world, bool raining, bool thundering, bool wasRaining,
                           bool wasThundering)>
            weatherChangeAllowed;
        std::function<bool(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z)>
            lightningAllowed;
    };

    /// The storm's own stream, which is not the level's: a strike must not shift what else spawns.
    struct Options {
        std::function<std::int64_t()> nextSeed;
    };

    using Worlds = std::function<std::vector<world::LoadedWorld*>()>;

    EnvironmentTickSystem(player::PlayerRegistry& players, const config::WorldsConfig& worldsConfig, Worlds worlds,
                          ChunkSimulationView& chunkSimulation, BlockChangeBroadcaster& broadcaster,
                          const command::CommandEnvironment& environment, NaturalSpawningSystem& spawning, Hooks hooks,
                          Options options, wire::common::Logger logger = wire::common::Logger{"World"});

    /// Skips the night when enough of the world's players have held a bed for five seconds.
    void tickSleep(world::LoadedWorld& world);

    /// Hurts whoever has walked outside the border.
    void tickBorderDamage(world::LoadedWorld& world);

    /// Moves whoever has stood in a portal long enough, and counts down the wait for everyone else.
    void tickPortals(world::LoadedWorld& world);

    /// Drowning, burning, and the two numbers the HUD draws them from.
    void tickSubmersion(world::LoadedWorld& world);

    /// Advances one world's clock and weather, and tells its players about it once a second.
    void tickTime(world::LoadedWorld& world);

private:
    /// The world a portal out of `world` leads to, or nothing with a line in the log saying why.
    [[nodiscard]] world::LoadedWorld* counterpart(const world::LoadedWorld& world, config::WorldDimension dimension);

    [[nodiscard]] world::LoadedWorld* netherCounterpart(const world::LoadedWorld& world);

    [[nodiscard]] world::LoadedWorld* endCounterpart(const world::LoadedWorld& world);

    void travelThroughPortal(world::LoadedWorld& world, player::PlayerSession& player,
                             server::world::portal::PortalAxis axis);

    void travelThroughEndPortal(world::LoadedWorld& world, player::PlayerSession& player);

    /// The storm's own share of a tick: one roll per simulated chunk, and a strike where it lands.
    void strikeLightning(world::LoadedWorld& world);

    player::PlayerRegistry* players_;
    const config::WorldsConfig* worldsConfig_;
    Worlds worlds_;
    ChunkSimulationView* chunkSimulation_;
    BlockChangeBroadcaster* broadcaster_;
    const command::CommandEnvironment* environment_;
    NaturalSpawningSystem* spawning_;
    Hooks hooks_;
    Options options_;
    wire::common::Logger logger_;
    /// Which missing dimensions have already been reported, per world: a player standing in a portal
    /// asks this twenty times a second.
    std::set<std::pair<std::string, config::WorldDimension>> warnedMissingDimensions_;
};

} // namespace basalt::server::runtime
