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
        /// What a plugin says about a food level that changed on its own.
        std::function<std::optional<std::int32_t>(const player::PlayerSession& player, std::int32_t now,
                                                  std::int32_t before)>
            foodChanged;
    };

    /// The storm's own stream, which is not the level's: a strike must not shift what else spawns.
    struct Options {
        std::function<std::int64_t()> nextSeed;
        /// Where the advancement for standing in a block goes.
        world::AdvancementTriggers* advancementTriggers = nullptr;
        /// The word the config carries, read fresh every tick: an operator can change it.
        std::function<std::string()> difficulty;
    };

    using Worlds = std::function<std::vector<world::LoadedWorld*>()>;

    EnvironmentTickSystem(player::PlayerRegistry& players, const config::WorldsConfig& worldsConfig, Worlds worlds,
                          ChunkSimulationView& chunkSimulation, BlockChangeBroadcaster& broadcaster,
                          const command::CommandEnvironment& environment, NaturalSpawningSystem& spawning, Hooks hooks,
                          Options options, wire::common::Logger logger = wire::common::Logger{"World"});

    /// One tick of every player's own body in this world: the hurt cooldown, the swing clock and
    /// hunger. Here rather than in a session's own turn because only one of the two doors has one.
    void tickPlayers(world::LoadedWorld& world);

    /// Skips the night when enough of the world's players have held a bed for five seconds.
    void tickSleep(world::LoadedWorld& world);

    /// Hurts whoever has walked outside the border.
    void tickBorderDamage(world::LoadedWorld& world);

    /// Draws the border for whoever cannot draw it themselves. parity: nothing in the jar, which
    /// needs none: its own client knows where the border is.
    void tickBorderWall(world::LoadedWorld& world);

    /// Moves whoever has stood in a portal long enough, and counts down the wait for everyone else.
    void tickPortals(world::LoadedWorld& world);

    /// parity: `Entity.checkInsideBlocks` reaching `ServerPlayer.onInsideBlock`: the blocks a player
    /// stands in, every tick, which is what the boat recipes and the gateway advancement wait for.
    void tickInsideBlocks(world::LoadedWorld& world);

    /// parity: `HoneyBlock.entityInside` for a player: the slide, its criterion and its effects.
    void honeySlide(world::LoadedWorld& world, player::PlayerSession& player, std::int32_t x, std::int32_t y,
                    std::int32_t z, std::int32_t state);

    /// parity: `ServerPlayer.doTick`'s `tickCount % 20`, where the biome and structure criteria are
    /// asked about once a second rather than every tick.
    void tickLocation(world::LoadedWorld& world);

    /// Where an end gateway inside this box sends something, resting that gateway. Nothing when
    /// the box touches no gateway that is ready. parity: `EndGatewayBlock.entityInside`.
    [[nodiscard]] std::optional<std::array<double, 3>> enterGateway(world::LoadedWorld& world,
                                                                    const std::array<double, 6>& box);

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

    void travelThroughGateway(world::LoadedWorld& world, player::PlayerSession& player,
                              const std::array<std::int32_t, 3>& gateway);

    /// The first ready gateway inside the box, now resting.
    [[nodiscard]] std::optional<std::array<std::int32_t, 3>> touchGateway(world::LoadedWorld& world,
                                                                          const std::array<double, 6>& box);

    void tickGatewayCooldowns(const world::LoadedWorld& world);

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
    /// parity: `TheEndGatewayBlockEntity.teleportCooldown`, by world and position.
    std::map<std::pair<std::string, std::array<std::int32_t, 3>>, std::int32_t> gatewayCooldowns_;
    std::mutex gatewayLock_;
};

} // namespace basalt::server::runtime
