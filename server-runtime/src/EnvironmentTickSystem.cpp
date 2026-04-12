#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/EnvironmentTickSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/player/PlayerEntityData.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerVitals.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/EndGateways.hpp>
#include <basalt/server/runtime/world/EndPortals.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/server/runtime/world/NetherPortals.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/SkeletonHorseTrap.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using server::world::portal::PortalAxis;
using wire::protocol::play::Gamemode;

/// parity: `Entity.lavaHurt`, which hurts every ten ticks, and fire, which does every twenty.
constexpr std::int64_t kLavaDamageIntervalTicks = 10;
constexpr std::int32_t kFireDamageIntervalTicks = 20;

/// parity: vanilla's own clock resync interval, once a second.
constexpr std::int64_t kTimeSyncIntervalTicks = 20;

/// parity: `ServerPlayer.tick` checks the border every ten ticks, not every one.
constexpr std::int64_t kBorderDamageIntervalTicks = 10;

/// parity: `ServerPlayer.doTick` asks the location criteria every twentieth tick.
constexpr std::int64_t kLocationIntervalTicks = 20;

constexpr double kPlayerHalfWidth = 0.3;
constexpr double kPlayerHeight = 1.8;
/// parity: `Entity.checkInsideBlocks` shrinks the box this much before asking the blocks.
constexpr double kInsideMargin = 1.0E-5;
/// parity: `PlayerList.broadcast` for a block event, sixty-four blocks.
constexpr double kBlockEventRange = 64.0;

} // namespace

EnvironmentTickSystem::EnvironmentTickSystem(player::PlayerRegistry& players, const config::WorldsConfig& worldsConfig,
                                             Worlds worlds, ChunkSimulationView& chunkSimulation,
                                             BlockChangeBroadcaster& broadcaster,
                                             const command::CommandEnvironment& environment,
                                             NaturalSpawningSystem& spawning, Hooks hooks, Options options,
                                             wire::common::Logger logger)
    : players_(&players), worldsConfig_(&worldsConfig), worlds_(std::move(worlds)), chunkSimulation_(&chunkSimulation),
      broadcaster_(&broadcaster), environment_(&environment), spawning_(&spawning), hooks_(std::move(hooks)),
      options_(std::move(options)), logger_(std::move(logger)) {}

void EnvironmentTickSystem::tickPlayers(world::LoadedWorld& world) {
    player::VitalsContext vitals;
    vitals.difficulty = player::difficultyOf(options_.difficulty ? options_.difficulty() : std::string{"easy"});
    vitals.foodChanged = hooks_.foodChanged;
    vitals.naturalRegeneration = world.gameRules().get(world::gameRules::kNaturalHealthRegeneration);
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            player::tickVitals(*player, vitals);
        }
    }
}

void EnvironmentTickSystem::tickSleep(world::LoadedWorld& world) {
    std::vector<PlayerSession*> players;
    std::vector<PlayerSession*> awake;
    std::vector<PlayerSession*> sleeping;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName != world.name()) {
            continue;
        }
        players.push_back(player);
        // parity: `Player.tick`: the counter stops at a hundred in bed, and once up it runs on to
        // a hundred and ten before it clears, which is the window a cat's gift is read in.
        if (!player->sleepingBed.has_value() && player->sleepTicks > 0 && ++player->sleepTicks >= 110) {
            player->sleepTicks = 0;
        }
        if (player->gamemode == Gamemode::Spectator) {
            continue;
        }
        awake.push_back(player);
        if (player->sleepingBed.has_value()) {
            // parity: `ServerPlayer.tick` counts this while a player is in bed. Counted here rather
            // than in the Java session's own turn because a Bedrock player has no such turn, and a
            // sleeper nobody counts never sees morning.
            player->sleepTicks = std::min(player->sleepTicks + 1, 100);
            sleeping.push_back(player);
        }
    }
    if (awake.empty() || sleeping.empty()) {
        return;
    }
    const std::int32_t percentage = world.gameRules().number(world::gameRules::kPlayersSleepingPercentage);
    const auto needed = std::max(
        std::size_t{1}, static_cast<std::size_t>(((awake.size() * static_cast<std::size_t>(percentage)) + 99) / 100));
    if (sleeping.size() < needed) {
        return;
    }
    for (const PlayerSession* player : sleeping) {
        if (player->sleepTicks < world::beds::kSleepTicksBeforeMorning) {
            return;
        }
    }
    world.clock().setTotalTicks(world::beds::nextMorning(world.clock().totalTicks()));
    world.weather().raining = false;
    world.weather().thundering = false;
    for (PlayerSession* player : players) {
        if (player->connection().wakeUp) {
            player->connection().wakeUp();
        }
        if (player->connection().sendTime) {
            player->connection().sendTime(world);
        }
    }
}

void EnvironmentTickSystem::tickBorderDamage(world::LoadedWorld& world) {
    if (world.gameTime() % kBorderDamageIntervalTicks != 0) {
        return;
    }
    const world::WorldBorder& border = world.border();
    if (border.damagePerBlock <= 0.0) {
        return;
    }
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName != world.name() || player->gamemode == Gamemode::Creative ||
            player->gamemode == Gamemode::Spectator) {
            continue;
        }
        const float damage = border.damageAt(player->x, player->z);
        if (damage <= 0.0F || !player->connection().hurt) {
            continue;
        }
        player->connection().hurt(damage, "outOfWorldBorder", world::damageTypes::kOutsideBorder, std::nullopt);
    }
}

void EnvironmentTickSystem::tickBorderWall(world::LoadedWorld& world) {
    /// The wall is a puff of particles that does not move, so once a second is enough; what stands
    /// in the way has to keep up with the player and goes every turn.
    constexpr std::int64_t kWallIntervalTicks = 20;

    const world::WorldBorder& border = world.border();
    const double half = border.size() / 2.0;
    const bool withWall = world.gameTime() % kWallIntervalTicks == 0;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName != world.name() || !player->connection().showBorder) {
            continue;
        }
        player->connection().showBorder(border.centerX() - half, border.centerX() + half, border.centerZ() - half,
                                        border.centerZ() + half, border.warningBlocks, withWall);
    }
}

void EnvironmentTickSystem::tickLocation(world::LoadedWorld& world) {
    if (options_.advancementTriggers == nullptr || world.gameTime() % kLocationIntervalTicks != 0) {
        return;
    }
    // parity: `PlayerTrigger` as `location`: the player's own predicate, where they stand.
    for (const PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            options_.advancementTriggers->fire("location", player->name(), player->uuid().toString());
        }
    }
}

// parity: the jar reaches `onInsideBlock` from `Entity.move`, so it asks on every movement packet
// rather than on a clock. Asking once a tick instead grants the same criteria a moment earlier at
// worst: what a criterion reads is the block, and a player who stands in one was moved into it.
void EnvironmentTickSystem::tickInsideBlocks(world::LoadedWorld& world) {
    if (options_.advancementTriggers == nullptr || !options_.advancementTriggers->listensTo("enter_block")) {
        return;
    }
    const auto low = [](double value) { return static_cast<std::int32_t>(std::floor(value + kInsideMargin)); };
    const auto high = [](double value) { return static_cast<std::int32_t>(std::floor(value - kInsideMargin)); };
    for (const PlayerSession* player : players_->snapshot()) {
        if (player->worldName != world.name()) {
            continue;
        }
        for (std::int32_t x = low(player->x - kPlayerHalfWidth); x <= high(player->x + kPlayerHalfWidth); ++x) {
            for (std::int32_t y = low(player->y); y <= high(player->y + kPlayerHeight); ++y) {
                for (std::int32_t z = low(player->z - kPlayerHalfWidth); z <= high(player->z + kPlayerHalfWidth); ++z) {
                    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
                    // parity: `onInsideBlock` is behind `insideBlock`, so air is never handed over.
                    if (!state.has_value() || registry::blocks::isAir(*state)) {
                        continue;
                    }
                    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*state);
                    if (block != nullptr) {
                        options_.advancementTriggers->fire(
                            "enter_block", player->name(), player->uuid().toString(),
                            world::TriggerEvent{.ids = {{"block", *block}},
                                                .state = registry::blockStateDefinitions::propertiesOf(*state)});
                    }
                }
            }
        }
    }
}

void EnvironmentTickSystem::tickPortals(world::LoadedWorld& world) {
    const std::int32_t creativeDelay = world.gameRules().number(world::gameRules::kPlayersNetherPortalCreativeDelay);
    const std::int32_t defaultDelay = world.gameRules().number(world::gameRules::kPlayersNetherPortalDefaultDelay);
    tickGatewayCooldowns(world);
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName != world.name()) {
            continue;
        }
        if (const std::optional<std::array<std::int32_t, 3>> gateway = touchGateway(
                world, {player->x - kPlayerHalfWidth, player->y, player->z - kPlayerHalfWidth,
                        player->x + kPlayerHalfWidth, player->y + kPlayerHeight, player->z + kPlayerHalfWidth});
            gateway.has_value() && !player->dead) {
            travelThroughGateway(world, *player, *gateway);
            continue;
        }
        if (player->portalCooldown > 0) {
            player->portalCooldown--;
            player->portalTicks = 0;
            continue;
        }
        const std::optional<std::int32_t> state = world.state().blockState(
            static_cast<std::int32_t>(std::floor(player->x)), static_cast<std::int32_t>(std::floor(player->y)),
            static_cast<std::int32_t>(std::floor(player->z)));
        if (state.has_value() && world::endPortals::isEndPortal(*state)) {
            // parity: `Portal.getPortalTransitionTime` is zero for an end portal, so it takes
            // whoever stepped in on the tick they stepped in.
            travelThroughEndPortal(world, *player);
            continue;
        }
        const std::optional<PortalAxis> axis =
            state.has_value() ? server::world::portal::PortalShape::axisOf(*state) : std::nullopt;
        if (!axis.has_value()) {
            player->portalTicks = 0;
            continue;
        }
        player->portalTicks++;
        const std::int32_t delay = player->gamemode == Gamemode::Creative ? creativeDelay : defaultDelay;
        if (player->portalTicks >= delay) {
            travelThroughPortal(world, *player, *axis);
        }
    }
}

void EnvironmentTickSystem::tickSubmersion(world::LoadedWorld& world) {
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName != world.name() || player->dead) {
            continue;
        }
        player::tickBody(*player, [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
            return world.state().blockState(x, y, z);
        });
        const double eyeY = player->y + world::fluids::eyeHeight(player->sneaking);
        const auto blockX = static_cast<std::int32_t>(std::floor(player->x));
        const auto blockZ = static_cast<std::int32_t>(std::floor(player->z));
        const std::optional<std::int32_t> feet =
            world.state().blockState(blockX, static_cast<std::int32_t>(std::floor(player->y)), blockZ);
        const std::optional<std::int32_t> eyes =
            world.state().blockState(blockX, static_cast<std::int32_t>(std::floor(eyeY)), blockZ);
        const std::int32_t air = player->airSupply;
        const std::int32_t fire = player->fireTicks;

        const bool drowns =
            !player->invulnerable() && eyes.has_value() && world::fluids::of(*eyes) == world::Submersion::Water;
        player->airSupply = world::fluids::airAfterTick(player->airSupply, drowns);
        if (drowns && player->airSupply <= world::fluids::kDrowningAir) {
            player->airSupply = 0;
            if (player->connection().hurt) {
                player->connection().hurt(world::fluids::kDrownDamage, "drown", world::damageTypes::kDrown,
                                          std::nullopt);
            }
        }

        const bool inLava =
            !player->invulnerable() && ((feet.has_value() && world::fluids::of(*feet) == world::Submersion::Lava) ||
                                        (eyes.has_value() && world::fluids::of(*eyes) == world::Submersion::Lava));
        if (inLava) {
            player->fireTicks = world::fluids::kLavaFireTicks;
            if (world.gameTime() % kLavaDamageIntervalTicks == 0 && player->connection().hurt) {
                player->connection().hurt(world::fluids::kLavaDamage, "lava", world::damageTypes::kLava, std::nullopt);
            }
        } else if (player->fireTicks > 0) {
            player->fireTicks--;
            // parity: `Entity.baseTick` burns for one heart every second, and lava's own damage
            // replaces it while standing in it.
            if (player->fireTicks % kFireDamageIntervalTicks == 0 && player->connection().hurt) {
                player->connection().hurt(world::fluids::kFireDamage, "onFire", world::damageTypes::kOnFire,
                                          std::nullopt);
            }
        }

        if ((player->airSupply != air || (player->fireTicks > 0) != (fire > 0)) &&
            player->connection().syncSubmersion) {
            player->connection().syncSubmersion();
        }
    }
}

world::LoadedWorld* EnvironmentTickSystem::counterpart(const world::LoadedWorld& world,
                                                       config::WorldDimension dimension) {
    const config::WorldDeclaration* declaration = worldsConfig_->portalTarget(world.declaration(), dimension);
    world::LoadedWorld* target = nullptr;
    if (declaration != nullptr) {
        for (world::LoadedWorld* candidate : worlds_()) {
            if (candidate != nullptr && candidate->name() == declaration->name) {
                target = candidate;
                break;
            }
        }
    }
    // A dimension nobody declared leaves the portal leading nowhere, and saying nothing about it is
    // the one answer that leaves the operator guessing.
    if (target == nullptr && warnedMissingDimensions_.emplace(world.name(), dimension).second) {
        logger_.warn([&] {
            const std::string overworld = worldsConfig_->overworldOf(world.declaration()).name;
            return "A portal in '" + world.name() + "' leads to the " +
                   std::string{config::dimensionFacts(dimension).configName} + " of '" + overworld +
                   "', and worlds.toml declares none, so it leads nowhere. Declare one with linked_to = \"" +
                   overworld + "\" to travel through it.";
        });
    }
    return target;
}

world::LoadedWorld* EnvironmentTickSystem::netherCounterpart(const world::LoadedWorld& world) {
    return counterpart(world, world.declaration().dimension == config::WorldDimension::Nether
                                  ? config::WorldDimension::Overworld
                                  : config::WorldDimension::Nether);
}

world::LoadedWorld* EnvironmentTickSystem::endCounterpart(const world::LoadedWorld& world) {
    return counterpart(world, world.declaration().dimension == config::WorldDimension::End
                                  ? config::WorldDimension::Overworld
                                  : config::WorldDimension::End);
}

void EnvironmentTickSystem::travelThroughPortal(world::LoadedWorld& world, PlayerSession& player, PortalAxis axis) {
    world::LoadedWorld* target = netherCounterpart(world);
    if (target == nullptr) {
        return;
    }
    // parity: `Level.isAllowedToEnterPortal`, the game rule that keeps a server out of the Nether.
    if (target->declaration().dimension == config::WorldDimension::Nether &&
        !world.gameRules().get(world::gameRules::kAllowEnteringNetherUsingPortals)) {
        return;
    }
    const std::optional<world::PortalExit> exit =
        world::netherPortals::exitFor(world, *target, player.x, player.y, player.z, axis);
    if (!exit.has_value()) {
        return;
    }
    if (hooks_.portalAllowed && !hooks_.portalAllowed(player, world, *target, exit->x, exit->y, exit->z, "nether")) {
        return;
    }
    player.portalTicks = 0;
    player.portalCooldown = world::netherPortals::kTravelCooldownTicks;
    if (player.connection().switchWorld) {
        static_cast<void>(player.connection().switchWorld(*target, exit->x, exit->y, exit->z));
    }
}

void EnvironmentTickSystem::travelThroughEndPortal(world::LoadedWorld& world, PlayerSession& player) {
    world::LoadedWorld* target = endCounterpart(world);
    if (target == nullptr) {
        return;
    }
    const world::endPortals::Exit exit =
        world::endPortals::exitFor(target->state(), world.declaration().dimension == config::WorldDimension::End,
                                   target->metadata().spawnX, target->metadata().spawnY, target->metadata().spawnZ);
    if (hooks_.portalAllowed && !hooks_.portalAllowed(player, world, *target, exit.x, exit.y, exit.z, "end")) {
        return;
    }
    player.portalTicks = 0;
    player.portalCooldown = world::netherPortals::kTravelCooldownTicks;
    if (player.connection().switchWorld) {
        static_cast<void>(player.connection().switchWorld(*target, exit.x, exit.y, exit.z));
    }
}

void EnvironmentTickSystem::travelThroughGateway(world::LoadedWorld& world, PlayerSession& player,
                                                 const std::array<std::int32_t, 3>& gateway) {
    // parity: `Entity.setAsInsidePortal`, where a player still resting only rests longer.
    if (player.portalCooldown > 0) {
        player.portalCooldown = world::endGateways::kPlayerDelayTicks;
        return;
    }
    const std::optional<std::array<double, 3>> exit =
        world::endGateways::destination(world.state(), gateway[0], gateway[1], gateway[2],
                                        world.declaration().dimension == config::WorldDimension::End);
    if (!exit.has_value() || (hooks_.portalAllowed && !hooks_.portalAllowed(player, world, world, (*exit)[0],
                                                                            (*exit)[1], (*exit)[2], "end_gateway"))) {
        return;
    }
    player.portalTicks = 0;
    player.portalCooldown = world::endGateways::kPlayerDelayTicks;
    // parity: `EndGatewayBlock.getPortalDestination` keeps the rotation, relative to nothing.
    if (player.connection().teleportTo) {
        player.connection().teleportTo((*exit)[0], (*exit)[1], (*exit)[2], player.yaw, player.pitch);
    }
}

std::optional<std::array<std::int32_t, 3>> EnvironmentTickSystem::touchGateway(world::LoadedWorld& world,
                                                                               const std::array<double, 6>& box) {
    const auto low = [](double value) { return static_cast<std::int32_t>(std::floor(value + kInsideMargin)); };
    const auto high = [](double value) { return static_cast<std::int32_t>(std::floor(value - kInsideMargin)); };
    for (std::int32_t x = low(box[0]); x <= high(box[3]); ++x) {
        for (std::int32_t y = low(box[1]); y <= high(box[4]); ++y) {
            for (std::int32_t z = low(box[2]); z <= high(box[5]); ++z) {
                const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
                if (!state.has_value() || !world::endGateways::isGateway(*state)) {
                    continue;
                }
                {
                    const std::scoped_lock guard{gatewayLock_};
                    std::int32_t& cooldown = gatewayCooldowns_[{world.name(), {x, y, z}}];
                    if (cooldown > 0) {
                        continue;
                    }
                    cooldown = world::endGateways::kCooldownTicks;
                }
                for (PlayerSession* watcher : players_->snapshot()) {
                    const double dx = watcher->x - x;
                    const double dy = watcher->y - y;
                    const double dz = watcher->z - z;
                    if (watcher->worldName == world.name() && watcher->connection().sendBlockEvent &&
                        (dx * dx) + (dy * dy) + (dz * dz) < kBlockEventRange * kBlockEventRange) {
                        watcher->connection().sendBlockEvent(x, y, z, *state, world::endGateways::kBeamEvent, 0);
                    }
                }
                return std::array<std::int32_t, 3>{x, y, z};
            }
        }
    }
    return std::nullopt;
}

std::optional<std::array<double, 3>> EnvironmentTickSystem::enterGateway(world::LoadedWorld& world,
                                                                         const std::array<double, 6>& box) {
    const std::optional<std::array<std::int32_t, 3>> gateway = touchGateway(world, box);
    if (!gateway.has_value()) {
        return std::nullopt;
    }
    return world::endGateways::destination(world.state(), (*gateway)[0], (*gateway)[1], (*gateway)[2],
                                           world.declaration().dimension == config::WorldDimension::End);
}

void EnvironmentTickSystem::tickGatewayCooldowns(const world::LoadedWorld& world) {
    const std::scoped_lock guard{gatewayLock_};
    std::erase_if(gatewayCooldowns_,
                  [&world](auto& entry) { return entry.first.first == world.name() && --entry.second <= 0; });
}

void EnvironmentTickSystem::tickTime(world::LoadedWorld& world) {
    const bool wasRaining = world.weather().raining;
    const bool wasThundering = world.weather().thundering;
    random::RandomSource weatherRandom = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    world.tickTime(weatherRandom);
    if (world.weather().raining != wasRaining || world.weather().thundering != wasThundering) {
        if (hooks_.weatherChangeAllowed &&
            !hooks_.weatherChangeAllowed(world, world.weather().raining, world.weather().thundering, wasRaining,
                                         wasThundering)) {
            world.weather().raining = wasRaining;
            world.weather().thundering = wasThundering;
        }
        environment_->syncWeather(world);
    }
    if (world.gameTime() % kTimeSyncIntervalTicks == 0) {
        environment_->syncTime(world);
    }
    if (world.weather().thundering) {
        strikeLightning(world);
    }
}

void EnvironmentTickSystem::strikeLightning(world::LoadedWorld& world) {
    if (!world.weather().raining) {
        return;
    }
    const std::vector<std::array<std::int32_t, 2>> chunks = chunkSimulation_->simulatedChunks(world);
    if (chunks.empty()) {
        return;
    }
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    std::vector<PlayerSession*> watchers;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    for (const std::array<std::int32_t, 2>& chunk : chunks) {
        if (!world::lightning::shouldStrike(random)) {
            continue;
        }
        const std::int32_t x = (chunk[0] << 4) + random.nextInt(16);
        const std::int32_t z = (chunk[1] << 4) + random.nextInt(16);
        const std::optional<std::array<std::int32_t, 3>> target = world::lightning::targetFor(world.state(), x, z);
        if (!target.has_value()) {
            continue;
        }
        if (hooks_.lightningAllowed && !hooks_.lightningAllowed(world, (*target)[0], (*target)[1], (*target)[2])) {
            continue;
        }
        const bool isTrap =
            world::skeletonHorseTrap::shouldCreateFromThunder(world, (*target)[0], (*target)[1], (*target)[2], random);
        if (isTrap) {
            static_cast<void>(spawning_->spawnSkeletonTrap(world, (*target)[0], (*target)[1], (*target)[2]));
        }
        std::vector<world::BlockChange> changes;
        // parity: `LightningBolt.tick`, where fire is only lit on normal and hard.
        const bool lightFires = !isTrap && (world.difficulty() == "normal" || world.difficulty() == "hard");
        world::lightning::strike(world.state(), (*target)[0], (*target)[1], (*target)[2], lightFires, random, changes);
        // parity: `GameEvent.LIGHTNING_STRIKE`, which a sculk sensor hears as fourteen.
        world::vibrations::emit(world.state(), "lightning_strike", static_cast<double>((*target)[0]),
                                static_cast<double>((*target)[1]), static_cast<double>((*target)[2]), world.gameTime());
        for (const world::BlockChange& change : changes) {
            broadcaster_->announceRedstone(world, watchers, change);
        }
    }
}

} // namespace basalt::server::runtime
