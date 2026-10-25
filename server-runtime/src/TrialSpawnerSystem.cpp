#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/MobSpawnerSystem.hpp>
#include <basalt/server/runtime/TrialSpawnerSystem.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/RealTrialSpawnerConfigs.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <algorithm>
#include <cmath>
#include <expected>
#include <map>
#include <string>
#include <unordered_map>

namespace basalt::server::runtime {
namespace {

namespace ts = world::trialSpawners;
using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;
using wire::common::Uuid;

constexpr const char* kStateProperty = "trial_spawner_state";
constexpr const char* kOminousProperty = "ominous";
/// parity: `TrialSpawner.MAX_MOB_TRACKING_DISTANCE`.
constexpr double kTrackingDistance = 47.0;
/// parity: `TrialSpawnerStateData.DELAY_BETWEEN_PLAYER_SCANS`.
constexpr std::int64_t kScanInterval = 20;
/// parity: `TrialSpawner.DETECT_PLAYER_SPAWN_BUFFER`.
constexpr std::int64_t kSpawnBuffer = 40;
/// parity: `TrialSpawnerState.DELAY_BEFORE_EJECT_AFTER_KILLING_LAST_MOB` and `TIME_BETWEEN_EACH_EJECTION`.
constexpr std::int64_t kShutterDelay = 40;
constexpr std::int64_t kEjectionInterval = 30;
/// parity: `TrialSpawnerStateData.TRIAL_OMEN_PER_BAD_OMEN_LEVEL`.
constexpr std::int32_t kTrialOmenPerBadOmenLevel = 18000;
/// parity: `TrialSpawnerConfig.ticksBetweenItemSpawners`.
constexpr std::int64_t kTicksBetweenItemSpawners = 160;
/// parity: `ServerLevel.levelEvent`, heard by the players within sixty-four blocks.
constexpr double kLevelEventRange = 64.0;
constexpr std::int32_t kDetectedPlayerEvent = 3013;
constexpr std::int32_t kEjectEvent = 3014;
constexpr std::int32_t kDetectedPlayerOminousEvent = 3019;
constexpr std::int32_t kBecomeOminousEvent = 3020;
constexpr std::int32_t kTrialSpawnedMobEvent = 3012;
constexpr double kEjectHeight = 1.2;
constexpr std::int32_t kEjectAccuracy = 2;

[[nodiscard]] std::int64_t packedPos(const std::array<std::int32_t, 3>& pos) {
    return ((static_cast<std::int64_t>(pos[0]) & 0x3ffffff) << 38) |
           ((static_cast<std::int64_t>(pos[2]) & 0x3ffffff) << 12) | (static_cast<std::int64_t>(pos[1]) & 0xfff);
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const char* name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] double eyeHeightOf(const PlayerSession& player) { return player.sneaking ? 1.27 : 1.62; }

/// parity: `PlayerDetector.inLineOfSight`: the ray from the eye may end in the spawner itself.
[[nodiscard]] bool inLineOfSight(world::WorldState& world, const std::array<std::int32_t, 3>& pos, double x, double y,
                                 double z) {
    const double centerX = pos[0] + 0.5;
    const double centerY = pos[1] + 0.5;
    const double centerZ = pos[2] + 0.5;
    const double dx = x - centerX;
    const double dy = y - centerY;
    const double dz = z - centerZ;
    const double widest = std::max({std::abs(dx), std::abs(dy), std::abs(dz)});
    if (widest < 1.0E-9) {
        return true;
    }
    const double scale = (0.5 + 1.0E-4) / widest;
    return !entity::blockRay::clips(world, x, y, z, centerX + (dx * scale), centerY + (dy * scale),
                                    centerZ + (dz * scale), entity::blockRay::ClipShape::Visual, {});
}

[[nodiscard]] bool tracked(const ServerEntity* mob, const std::array<std::int32_t, 3>& pos) {
    if (mob == nullptr || mob->removed || mob->killed || mob->health <= 0.0) {
        return false;
    }
    const double dx = std::floor(mob->x) - pos[0];
    const double dy = std::floor(mob->y) - pos[1];
    const double dz = std::floor(mob->z) - pos[2];
    return (dx * dx) + (dy * dy) + (dz * dz) <= kTrackingDistance * kTrackingDistance;
}

} // namespace

struct TrialSpawnerSystem::Turn {
    world::LoadedWorld& world;
    std::array<std::int32_t, 3> pos;
    std::int32_t stateId = 0;
    bool ominous = false;
    ts::Full config;
    ts::Data data;
    random::RandomSource random;
    std::int64_t gameTime = 0;
    std::vector<PlayerSession*> watchers;
    std::unordered_map<Uuid, ServerEntity*> mobsByUuid;

    [[nodiscard]] const ts::Config& active() const { return ominous ? config.ominous : config.normal; }
};

TrialSpawnerSystem::TrialSpawnerSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster,
                                       MobSpawnerSystem& mobs, Options options)
    : players_(&players), broadcaster_(&broadcaster), mobs_(&mobs), options_(std::move(options)) {}

bool TrialSpawnerSystem::tick(world::LoadedWorld& world, const std::array<std::int32_t, 3>& pos) {
    const std::optional<std::int32_t> stateId = world.state().blockState(pos[0], pos[1], pos[2]);
    const wire::common::Identifier* block =
        stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    server::world::Chunk* chunk = world.state().chunkIfLoaded(pos[0] >> 4, pos[2] >> 4);
    server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(pos[0] & 15, pos[1], pos[2] & 15);
    if (block == nullptr || block->path() != "trial_spawner" || entity == nullptr) {
        return false;
    }
    const std::optional<ts::State> current = ts::stateByName(propertyOf(*stateId, kStateProperty));
    if (!current.has_value()) {
        return false;
    }
    const world::RealTrialSpawnerConfigs* configs = options_.configs;
    const ts::ConfigLookup lookup = [configs](const Identifier& id) {
        return configs == nullptr ? nullptr : configs->definitionOf(id);
    };
    Turn turn{.world = world,
              .pos = pos,
              .stateId = *stateId,
              .ominous = propertyOf(*stateId, kOminousProperty) == "true",
              .config = ts::readFull(entity->data(), lookup),
              .data = ts::Data::read(entity->data()),
              .random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0),
              .gameTime = world.gameTime()};
    const ts::Data before = turn.data;
    // parity: `shouldMobBeUntracked`.
    if (!turn.data.currentMobs.empty()) {
        for (ServerEntity* mob : world.state().entities()) {
            turn.mobsByUuid.emplace(mob->uuid(), mob);
        }
        const std::size_t held = turn.data.currentMobs.size();
        std::erase_if(turn.data.currentMobs, [&turn](const Uuid& id) {
            const auto found = turn.mobsByUuid.find(id);
            return !tracked(found == turn.mobsByUuid.end() ? nullptr : found->second, turn.pos);
        });
        if (turn.data.currentMobs.size() != held) {
            turn.data.nextMobSpawnsAt = turn.gameTime + turn.active().ticksBetweenSpawn;
        }
    }
    const ts::State next = step(turn, *current);
    if (next != *current) {
        setBlock(turn, kStateProperty, std::string{ts::nameOf(next)});
    }
    if (turn.data == before) {
        return false;
    }
    // The turn may have rewritten the chunk's block entities, so the spawner is looked up again.
    server::world::ChunkBlockEntity* again = chunk->blockEntityAt(pos[0] & 15, pos[1], pos[2] & 15);
    if (again == nullptr) {
        return false;
    }
    turn.data.write(again->data());
    return true;
}

bool TrialSpawnerSystem::canSpawnInLevel(const Turn& turn) const {
    // parity: `TrialSpawner.canSpawnInLevel`.
    const world::LoadedWorld& world = turn.world;
    if (!world.gameRules().get(world::gameRules::kSpawnerBlocksWork)) {
        return false;
    }
    return world.difficulty() != "peaceful" && world.gameRules().get(world::gameRules::kSpawnMobs);
}

ts::State TrialSpawnerSystem::step(Turn& turn, const ts::State current) {
    ts::Data& data = turn.data;
    const ts::Config& config = turn.active();
    switch (current) {
    case ts::State::Inactive: {
        const world::SpawnData& next = data.nextOrPick(config, turn.random);
        const wire::nbt::NbtTag* id = next.entity.find("id");
        const auto* name = id == nullptr ? nullptr : id->get<std::string>();
        const bool known = name != nullptr && Identifier::parse(*name).has_value() &&
                           registry::entityTypes::of(*Identifier::parse(*name)) != nullptr;
        return known ? ts::State::WaitingForPlayers : current;
    }
    case ts::State::WaitingForPlayers: {
        if (!canSpawnInLevel(turn)) {
            data.resetStatistics();
            return current;
        }
        if (!hasMobToSpawn(turn)) {
            return ts::State::Inactive;
        }
        detectPlayers(turn, current);
        return data.detectedPlayers.empty() ? current : ts::State::Active;
    }
    case ts::State::Active: {
        if (!canSpawnInLevel(turn)) {
            data.resetStatistics();
            return ts::State::WaitingForPlayers;
        }
        if (!hasMobToSpawn(turn)) {
            return ts::State::Inactive;
        }
        const auto additionalPlayers =
            static_cast<std::int32_t>(data.detectedPlayers.size() > 1 ? data.detectedPlayers.size() - 1 : 0);
        detectPlayers(turn, current);
        if (data.totalMobsSpawned >= config.targetTotalMobs(additionalPlayers)) {
            if (data.currentMobs.empty()) {
                data.cooldownEndsAt = turn.gameTime + turn.config.targetCooldownLength;
                data.totalMobsSpawned = 0;
                data.nextMobSpawnsAt = 0;
                return ts::State::WaitingForRewardEjection;
            }
        } else if (turn.gameTime >= data.nextMobSpawnsAt && static_cast<std::int32_t>(data.currentMobs.size()) <
                                                                config.targetSimultaneousMobs(additionalPlayers)) {
            const ts::Config& now = turn.active();
            const world::SpawnData spawn = data.nextOrPick(now, turn.random);
            if (const std::optional<Uuid> spawned =
                    mobs_->spawnForTrial(turn.world, spawn, turn.pos, now.spawnRange, turn.ominous, turn.random);
                spawned.has_value()) {
                data.currentMobs.push_back(*spawned);
                ++data.totalMobsSpawned;
                data.nextMobSpawnsAt = turn.gameTime + config.ticksBetweenSpawn;
                if (const world::SpawnData* picked = world::baseSpawnerData::pick(config.spawnPotentials, turn.random);
                    picked != nullptr) {
                    data.nextSpawnData = *picked;
                }
            }
        }
        return current;
    }
    case ts::State::WaitingForRewardEjection: {
        const std::int64_t startedAt = data.cooldownEndsAt - turn.config.targetCooldownLength;
        if (turn.gameTime >= startedAt + kShutterDelay) {
            sound(turn, "block.trial_spawner.open_shutter");
            return ts::State::EjectingReward;
        }
        return current;
    }
    case ts::State::EjectingReward: {
        const std::int64_t startedAt = data.cooldownEndsAt - turn.config.targetCooldownLength;
        if ((turn.gameTime - startedAt) % kEjectionInterval != 0) {
            return current;
        }
        if (data.detectedPlayers.empty()) {
            sound(turn, "block.trial_spawner.close_shutter");
            data.ejectingLootTable.reset();
            return ts::State::Cooldown;
        }
        if (!data.ejectingLootTable.has_value()) {
            if (const ts::WeightedTable* picked = ts::pickWeighted(config.lootTablesToEject, turn.random);
                picked != nullptr) {
                data.ejectingLootTable = picked->table;
            }
        }
        if (data.ejectingLootTable.has_value()) {
            ejectReward(turn, *data.ejectingLootTable);
        }
        data.detectedPlayers.erase(data.detectedPlayers.begin());
        return current;
    }
    case ts::State::Cooldown: {
        detectPlayers(turn, current);
        if (!data.detectedPlayers.empty()) {
            data.totalMobsSpawned = 0;
            data.nextMobSpawnsAt = 0;
            return ts::State::Active;
        }
        if (turn.gameTime >= data.cooldownEndsAt) {
            setBlock(turn, kOminousProperty, "false");
            turn.ominous = false;
            data.reset();
            return ts::State::WaitingForPlayers;
        }
        return current;
    }
    }
    return current;
}

bool TrialSpawnerSystem::hasMobToSpawn(Turn& turn) const {
    // parity: `TrialSpawnerStateData.hasMobToSpawn`.
    const world::SpawnData& next = turn.data.nextOrPick(turn.active(), turn.random);
    const wire::nbt::NbtTag* id = next.entity.find("id");
    return (id != nullptr && id->get<std::string>() != nullptr) || !turn.active().spawnPotentials.empty();
}

std::vector<Uuid> TrialSpawnerSystem::playersInRange(Turn& turn, const bool requireSight) const {
    std::vector<Uuid> found;
    const auto range = static_cast<double>(turn.config.requiredPlayerRange);
    for (PlayerSession* player : turn.watchers) {
        if (player->gamemode == wire::protocol::play::Gamemode::Creative ||
            player->gamemode == wire::protocol::play::Gamemode::Spectator) {
            continue;
        }
        const double dx = std::floor(player->x) - turn.pos[0];
        const double dy = std::floor(player->y) - turn.pos[1];
        const double dz = std::floor(player->z) - turn.pos[2];
        if ((dx * dx) + (dy * dy) + (dz * dz) >= range * range) {
            continue;
        }
        if (requireSight &&
            !inLineOfSight(turn.world.state(), turn.pos, player->x, player->y + eyeHeightOf(*player), player->z)) {
            continue;
        }
        found.push_back(player->uuid());
    }
    return found;
}

void TrialSpawnerSystem::detectPlayers(Turn& turn, const ts::State current) {
    // parity: `TrialSpawnerStateData.tryDetectPlayers`, once every twenty ticks for a spawner.
    if ((packedPos(turn.pos) + turn.gameTime) % kScanInterval != 0) {
        return;
    }
    if (current == ts::State::Cooldown && turn.ominous) {
        return;
    }
    turn.watchers = players_->snapshot();
    turn.watchers.erase(
        std::remove_if(turn.watchers.begin(), turn.watchers.end(),
                       [&turn](const PlayerSession* player) { return player->worldName != turn.world.name(); }),
        turn.watchers.end());
    const std::vector<Uuid> inSight = playersInRange(turn, true);
    bool becameOminous = false;
    if (!turn.ominous && !inSight.empty()) {
        PlayerSession* trial = nullptr;
        PlayerSession* bad = nullptr;
        for (const Uuid& id : inSight) {
            for (PlayerSession* player : turn.watchers) {
                if (player->uuid() != id) {
                    continue;
                }
                if (player->effects.contains(Identifier::vanilla("trial_omen"))) {
                    trial = player;
                } else if (player->effects.contains(world::mobEffects::badOmen()) && bad == nullptr) {
                    bad = player;
                }
            }
            if (trial != nullptr) {
                break;
            }
        }
        if (trial != nullptr || bad != nullptr) {
            becomeOminous(turn, trial != nullptr ? *trial : *bad);
            becameOminous = true;
        }
    }
    if (current == ts::State::Cooldown && !becameOminous) {
        return;
    }
    const bool searchingForFirst = turn.data.detectedPlayers.empty();
    const std::vector<Uuid> found = searchingForFirst ? inSight : playersInRange(turn, false);
    bool added = false;
    for (const Uuid& id : found) {
        if (std::ranges::find(turn.data.detectedPlayers, id) == turn.data.detectedPlayers.end()) {
            turn.data.detectedPlayers.push_back(id);
            added = true;
        }
    }
    if (added) {
        turn.data.nextMobSpawnsAt = std::max(turn.gameTime + kSpawnBuffer, turn.data.nextMobSpawnsAt);
        if (!becameOminous) {
            levelEvent(turn, turn.ominous ? kDetectedPlayerOminousEvent : kDetectedPlayerEvent, turn.pos,
                       static_cast<std::int32_t>(turn.data.detectedPlayers.size()));
        }
    }
}

void TrialSpawnerSystem::becomeOminous(Turn& turn, PlayerSession& trigger) {
    // parity: `transformBadOmenIntoTrialOmen`.
    const Identifier trialOmen = Identifier::vanilla("trial_omen");
    if (!trigger.effects.contains(trialOmen)) {
        if (const auto bad = trigger.effects.find(world::mobEffects::badOmen()); bad != trigger.effects.end()) {
            const std::int32_t amplifier = bad->second.amplifier + 1;
            trigger.effects.erase(bad);
            if (trigger.connection().sendEffectRemoved) {
                trigger.connection().sendEffectRemoved(world::mobEffects::badOmen());
            }
            if (trigger.connection().addEffect) {
                trigger.connection().addEffect(trialOmen, kTrialOmenPerBadOmenLevel * amplifier, 0);
            }
        }
    }
    levelEvent(turn, kBecomeOminousEvent,
               {static_cast<std::int32_t>(std::floor(trigger.x)),
                static_cast<std::int32_t>(std::floor(trigger.y + eyeHeightOf(trigger))),
                static_cast<std::int32_t>(std::floor(trigger.z))},
               0);
    // parity: `TrialSpawner.applyOminous` and `TrialSpawnerStateData.resetAfterBecomingOminous`.
    setBlock(turn, kOminousProperty, "true");
    levelEvent(turn, kBecomeOminousEvent, turn.pos, 1);
    turn.ominous = true;
    ts::Data& data = turn.data;
    if (turn.mobsByUuid.empty() && !data.currentMobs.empty()) {
        for (ServerEntity* mob : turn.world.state().entities()) {
            turn.mobsByUuid.emplace(mob->uuid(), mob);
        }
    }
    for (const Uuid& id : data.currentMobs) {
        const auto found = turn.mobsByUuid.find(id);
        if (found == turn.mobsByUuid.end() || found->second == nullptr) {
            continue;
        }
        levelEvent(turn, kTrialSpawnedMobEvent,
                   {static_cast<std::int32_t>(std::floor(found->second->x)),
                    static_cast<std::int32_t>(std::floor(found->second->y)),
                    static_cast<std::int32_t>(std::floor(found->second->z))},
                   0);
        found->second->removed = true;
    }
    if (!turn.config.ominous.spawnPotentials.empty()) {
        data.nextSpawnData.reset();
    }
    data.totalMobsSpawned = 0;
    data.currentMobs.clear();
    data.nextMobSpawnsAt = turn.gameTime + turn.config.ominous.ticksBetweenSpawn;
    data.cooldownEndsAt = turn.gameTime + kTicksBetweenItemSpawners;
}

void TrialSpawnerSystem::ejectReward(Turn& turn, const std::string& table) {
    // parity: `TrialSpawner.ejectReward` over `DefaultDispenseItemBehavior.spawnItem`.
    if (options_.lootTables == nullptr) {
        return;
    }
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(table);
    const server::world::loot::LootTable* rolled = id.has_value() ? options_.lootTables->byId(*id) : nullptr;
    if (rolled == nullptr) {
        return;
    }
    server::world::loot::LootContext context{};
    options_.lootTables->prepareAt(context, turn.world, turn.pos[0] + 0.5, turn.pos[1] + 0.5, turn.pos[2] + 0.5);
    const std::vector<server::world::loot::ItemStack> drops =
        rolled->generate(turn.random, 0.0F, std::nullopt, context);
    if (drops.empty()) {
        return;
    }
    const auto triangle = [&turn](double center, double spread) {
        const double first = turn.random.nextDouble();
        const double second = turn.random.nextDouble();
        return center + (spread * (first - second));
    };
    for (const server::world::loot::ItemStack& stack : drops) {
        const double spawnY = turn.pos[1] + kEjectHeight - 0.125;
        const double power = (turn.random.nextDouble() * 0.1) + 0.2;
        const double spread = 0.0172275 * kEjectAccuracy;
        world::DroppedItem item{.entityId = players_->allocateEntityId(),
                                .uuid = Uuid::random(),
                                .item = stack.item,
                                .count = stack.count,
                                .x = turn.pos[0] + 0.5,
                                .y = spawnY,
                                .z = turn.pos[2] + 0.5};
        item.components = stack.components;
        static_cast<void>(power);
        item.velocityX = triangle(0.0, spread);
        item.velocityY = triangle(0.2, spread);
        item.velocityZ = triangle(0.0, spread);
        static_cast<void>(turn.world.state().spawnItem(std::move(item)));
    }
    levelEvent(turn, kEjectEvent, turn.pos, 0);
}

void TrialSpawnerSystem::setBlock(Turn& turn, const char* property, const std::string& value) {
    const std::optional<std::int32_t> now = turn.world.state().blockState(turn.pos[0], turn.pos[1], turn.pos[2]);
    const wire::common::Identifier* block = now.has_value() ? registry::blockStateDefinitions::blockOf(*now) : nullptr;
    if (block == nullptr) {
        return;
    }
    const std::optional<std::int32_t> changed =
        registry::blockStateDefinitions::stateIdOf(*block, {{property, value}}, *now);
    if (!changed.has_value() || *changed == *now ||
        !turn.world.state().setBlockState(turn.pos[0], turn.pos[1], turn.pos[2], *changed)) {
        return;
    }
    turn.stateId = *changed;
    std::vector<PlayerSession*> watchers = players_->snapshot();
    std::erase_if(watchers, [&turn](const PlayerSession* player) { return player->worldName != turn.world.name(); });
    broadcaster_->announceRedstone(
        turn.world, watchers,
        world::BlockChange{.x = turn.pos[0], .y = turn.pos[1], .z = turn.pos[2], .stateId = *changed});
}

void TrialSpawnerSystem::levelEvent(Turn& turn, const std::int32_t type, const std::array<std::int32_t, 3>& at,
                                    const std::int32_t data) const {
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName != turn.world.name() || !player->connection().sendLevelEvent) {
            continue;
        }
        const double dx = at[0] + 0.5 - player->x;
        const double dy = at[1] + 0.5 - player->y;
        const double dz = at[2] + 0.5 - player->z;
        if ((dx * dx) + (dy * dy) + (dz * dz) < kLevelEventRange * kLevelEventRange) {
            player->connection().sendLevelEvent(type, at[0], at[1], at[2], data);
        }
    }
}

void TrialSpawnerSystem::sound(Turn& turn, const char* name) const {
    std::vector<PlayerSession*> watchers = players_->snapshot();
    std::erase_if(watchers, [&turn](const PlayerSession* player) { return player->worldName != turn.world.name(); });
    broadcaster_->sendEffects(watchers, turn.pos[0], turn.pos[1], turn.pos[2], turn.stateId,
                              {world::blockEffect::Sound{.name = name}});
}

} // namespace basalt::server::runtime
