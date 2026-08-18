#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/BossBarView.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/WitherSystem.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/DragonNavigation.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobOverrides.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/SummonAwards.hpp>
#include <basalt/server/runtime/world/WitherPatterns.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>

namespace basalt::server::runtime {
namespace {

/// parity: `WitherSkull.getBlockExplosionResistance`'s own ceiling.
constexpr float kDangerousResistance = 0.8F;

namespace boss = entity::witherBoss;
namespace mth = wire::common::mth;
namespace play = wire::protocol::play;
using entity::ServerEntity;
using player::PlayerSession;

/// parity: `LevelEvent.SOUND_WITHER_BLOCK_BREAK`.
constexpr std::int32_t kBlockBreakEvent = 1022;
constexpr std::int32_t kSpawnEvent = 1023;
constexpr std::int32_t kShootEvent = 1024;
constexpr std::int32_t kBlockDestroyedEvent = 2001;
constexpr double kLevelEventRange = 64.0;
/// parity: `ChunkMap` tracks a wither ten chunks out, and the bar follows the tracking.
constexpr double kTrackingRange = 160.0;
constexpr double kSpawnFootroom = 0.55;
constexpr float kBirthExplosion = 7.0F;
constexpr float kSkullExplosion = 1.0F;
constexpr float kSkullDamage = 8.0F;
constexpr float kUnownedSkullDamage = 5.0F;
constexpr double kKillHeal = 5.0;
constexpr double kBirthHeal = 10.0;
constexpr std::int32_t kWitherEffectAmplifier = 1;
/// parity: `AbstractHurtingProjectile.accelerationPower`, the speed a skull leaves at.
constexpr float kSkullSpeed = 0.1F;
constexpr double kHeadRange = 30.0;
constexpr float kDangerousChance = 0.001F;
constexpr double kPlayerEyeHeight = 1.62;
/// parity: `EntityDimensions.eyeHeight` for most mobs.
constexpr double kEyeShare = 0.85;

[[nodiscard]] bool targetable(const PlayerSession& player) {
    return !player.dead && player.gamemode != play::Gamemode::Creative && player.gamemode != play::Gamemode::Spectator;
}

[[nodiscard]] double eyeOf(const ServerEntity& entity) {
    return server::registry::entityTypeShapes::heightOf(entity.type()) * kEyeShare;
}

/// parity: `Mth.nextDouble`.
[[nodiscard]] double nextDouble(random::RandomSource& random, double min, double max) {
    return min >= max ? min : (random.nextDouble() * (max - min)) + min;
}

[[nodiscard]] wire::text::Component nameOf(const ServerEntity& entity) {
    return entity.customName.has_value()
               ? wire::text::Component::text(*entity.customName)
               : wire::text::Component::translatable("entity.minecraft." + entity.type().path());
}

void heal(ServerEntity& wither, double amount) {
    if (wither.health > 0.0) {
        wither.health = std::min(wither.health + amount, boss::kMaxHealth);
    }
}

} // namespace

WitherSystem::WitherSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster,
                           ExplosionSystem& explosions, Options options)
    : players_(&players), broadcaster_(&broadcaster), explosions_(&explosions), options_(std::move(options)) {}

WitherSystem::~WitherSystem() = default;

bool WitherSystem::isWither(const ServerEntity& entity) { return boss::isWither(entity.type()); }

boss::WitherState& WitherSystem::stateOf(ServerEntity& wither) {
    if (wither.wither == nullptr) {
        wither.wither = std::make_shared<boss::WitherState>();
        wither.wither->random = random::RandomSource::legacy(wither.entityId());
    }
    return *wither.wither;
}

bool WitherSystem::tagged(std::int32_t stateId, std::string_view tag) const {
    const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
    return options_.blockTags != nullptr && block != nullptr &&
           world::realBlockTags::contains(*options_.blockTags, tag, *block);
}

std::vector<PlayerSession*> WitherSystem::watchersOf(const world::LoadedWorld& world) const {
    std::vector<PlayerSession*> watchers;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    return watchers;
}

void WitherSystem::placed(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> standing = world.state().blockState(x, y, z);
    if (!standing.has_value() || !world::witherPatterns::isWitherSkull(*standing) || world.difficulty() == "peaceful") {
        return;
    }
    const std::optional<world::witherPatterns::Match> match =
        world::witherPatterns::find(world.state(), x, y, z, [this](std::int32_t stateId) {
            return tagged(stateId, "minecraft:wither_summon_base_blocks");
        });
    const server::registry::EntityTypeEntry* entry =
        server::registry::entityTypes::of(wire::common::Identifier::vanilla("wither"));
    if (!match.has_value() || entry == nullptr) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    // parity: `CarvedPumpkinBlock.clearPatternBlocks`.
    const std::int32_t air = server::registry::blocks::air().stateId;
    std::vector<std::int32_t> cleared;
    cleared.reserve(match->cells.size());
    for (const auto& cell : match->cells) {
        const std::int32_t was = world.state().blockState(cell[0], cell[1], cell[2]).value_or(air);
        cleared.push_back(was);
        if (world.state().setBlockState(cell[0], cell[1], cell[2], air)) {
            broadcaster_->announceBlock(
                watchers,
                world::BlockChange{.x = cell[0],
                                   .y = cell[1],
                                   .z = cell[2],
                                   .stateId = air,
                                   .effects = {world::blockEffect::Effect{.type = kBlockDestroyedEvent, .data = was}}});
        }
    }
    const float yaw = match->alongX ? 0.0F : 90.0F;
    auto wither = std::make_unique<ServerEntity>(players_->allocateEntityId(), wire::common::Uuid::random(), entry->id,
                                                 entry->protocolId, match->spawn[0] + 0.5,
                                                 match->spawn[1] + kSpawnFootroom, match->spawn[2] + 0.5, yaw, 0.0F);
    wither->headYaw = yaw;
    boss::WitherState& state = stateOf(*wither);
    state.random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    boss::makeInvulnerable(*wither, state);
    // parity: `WitherSkullBlock.checkSpawn` awards it before the wither is in the world.
    // player within fifty blocks of where it is about to stand.
    world::summonAwards::award(options_.advancementTriggers, watchers, world.name(), *wither,
                               world::summonAwards::kWitherRange);
    static_cast<void>(world.state().spawnEntity(std::move(wither)));
    // parity: `CarvedPumpkinBlock.updatePatternBlocks`.
    for (std::size_t index = 0; index < match->cells.size(); ++index) {
        const auto& cell = match->cells[index];
        broadcaster_->notifyAround(world, watchers, cell[0], cell[1], cell[2], cleared[index]);
    }
}

std::optional<WitherSystem::Aim> WitherSystem::aimAt(world::LoadedWorld& world, std::int32_t entityId) const {
    if (entityId <= 0) {
        return std::nullopt;
    }
    for (const PlayerSession* player : players_->snapshot()) {
        if (player->entityId() == entityId && player->worldName == world.name()) {
            return Aim{.entityId = entityId,
                       .x = player->x,
                       .y = player->y,
                       .z = player->z,
                       .height = kPlayerEyeHeight,
                       .alive = targetable(*player)};
        }
    }
    const ServerEntity* entity = world.state().entity(entityId);
    if (entity == nullptr) {
        return std::nullopt;
    }
    return Aim{.entityId = entityId,
               .x = entity->x,
               .y = entity->y,
               .z = entity->z,
               .height = eyeOf(*entity),
               .alive = !entity->removed && !entity->dying && entity->health > 0.0};
}

void WitherSystem::beforeTurn(world::LoadedWorld& world, const std::vector<ServerEntity*>& living,
                              ServerEntity& wither) {
    static_cast<void>(living);
    boss::WitherState& state = stateOf(wither);
    double vx = wither.velocityX;
    double vy = wither.velocityY * 0.6;
    double vz = wither.velocityZ;
    if (const std::optional<Aim> target = aimAt(world, state.targets[0]); target.has_value()) {
        if (wither.y < target->y || (!boss::isPowered(wither) && wither.y < target->y + 5.0)) {
            vy = std::max(0.0, vy);
            vy += 0.3 - (vy * static_cast<double>(0.6F));
        }
        const double dx = target->x - wither.x;
        const double dz = target->z - wither.z;
        const double flat = (dx * dx) + (dz * dz);
        if (flat > 9.0) {
            const double length = std::sqrt(flat);
            vx += ((dx / length) * 0.3) - (vx * 0.6);
            vz += ((dz / length) * 0.3) - (vz * 0.6);
        }
    }
    wither.velocityX = vx;
    wither.velocityY = vy;
    wither.velocityZ = vz;
    if ((vx * vx) + (vz * vz) > 0.05) {
        wither.yaw =
            (static_cast<float>(entity::enderDragon::vanillaAtan2(vz, vx)) * (180.0F / std::numbers::pi_v<float>)) -
            90.0F;
    }
}

void WitherSystem::shoot(world::LoadedWorld& world, ServerEntity& wither, std::int32_t head, double x, double y,
                         double z, bool dangerous) {
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    if (!(wither.sourceNbt.has_value() && wither.sourceNbt->getBoolean("Silent"))) {
        levelEvent(watchers, kShootEvent, wither);
    }
    const entity::projectiles::Kind* kind =
        entity::projectiles::kindOf(wire::common::Identifier::vanilla("wither_skull"));
    const server::registry::EntityTypeEntry* entry =
        kind != nullptr ? server::registry::entityTypes::of(kind->type) : nullptr;
    if (entry == nullptr) {
        return;
    }
    const std::array<double, 3> from = boss::headPosition(wither, head);
    random::RandomSource& random = stateOf(wither).random;
    ServerEntity& skull =
        entity::projectiles::spawn(world.state(), *kind, players_->allocateEntityId(), entry->protocolId, from[0],
                                   from[1], from[2], x - from[0], y - from[1], z - from[2], kSkullSpeed, 0.0F, random);
    skull.ownerId = wither.entityId();
    skull.dangerousSkull = dangerous;
}

void WitherSystem::shootAtTarget(world::LoadedWorld& world, ServerEntity& wither) {
    boss::WitherState& state = stateOf(wither);
    const std::optional<Aim> target = wither.targetId.has_value() ? aimAt(world, *wither.targetId) : std::nullopt;
    if (!target.has_value()) {
        return;
    }
    // parity: the middle head rolls for a blue skull, and only it.
    const bool dangerous = state.random.nextFloat() < kDangerousChance;
    shoot(world, wither, 0, target->x, target->y + (target->height * 0.5), target->z, dangerous);
}

void WitherSystem::tickHeads(world::LoadedWorld& world, const std::vector<ServerEntity*>& living, ServerEntity& wither,
                             boss::WitherState& state) {
    const bool fierce = world.difficulty() == "normal" || world.difficulty() == "hard";
    for (std::int32_t head = 1; head < 3; ++head) {
        const auto slot = static_cast<std::size_t>(head - 1);
        if (wither.tickCount < state.nextHeadUpdate[slot]) {
            continue;
        }
        state.nextHeadUpdate[slot] = wither.tickCount + 10 + state.random.nextInt(10);
        if (fierce && state.idleHeadUpdates[slot]++ > 15) {
            const double x = nextDouble(state.random, wither.x - 10.0, wither.x + 10.0);
            const double y = nextDouble(state.random, wither.y - 5.0, wither.y + 5.0);
            const double z = nextDouble(state.random, wither.z - 10.0, wither.z + 10.0);
            shoot(world, wither, head + 1, x, y, z, true);
            state.idleHeadUpdates[slot] = 0;
        }
        const auto index = static_cast<std::size_t>(head);
        if (state.targets[index] > 0) {
            const std::optional<Aim> current = aimAt(world, state.targets[index]);
            const bool inReach = current.has_value() && current->alive &&
                                 (std::pow(current->x - wither.x, 2) + std::pow(current->y - wither.y, 2) +
                                  std::pow(current->z - wither.z, 2)) <= kHeadRange * kHeadRange;
            const bool seen =
                inReach && !entity::blockRay::cast(world.state(), wither.x, wither.y + eyeOf(wither), wither.z,
                                                   current->x, current->y + current->height, current->z)
                                .has_value();
            if (seen) {
                shoot(world, wither, head + 1, current->x, current->y + (current->height * 0.5), current->z, false);
                state.nextHeadUpdate[slot] = wither.tickCount + 40 + state.random.nextInt(20);
                state.idleHeadUpdates[slot] = 0;
            } else {
                state.targets[index] = 0;
            }
            continue;
        }
        // parity: `getNearbyEntities` inside the box grown by 20, 8, 20, with the 20 block range.
        const double half = wither.width() / 2.0;
        const auto inBox = [&wither, half](double x, double y, double z, double height) {
            return x > wither.x - half - 20.0 && x < wither.x + half + 20.0 && y + height > wither.y - 8.0 &&
                   y < wither.y + wither.height() + 8.0 && z > wither.z - half - 20.0 && z < wither.z + half + 20.0 &&
                   std::pow(x - wither.x, 2) + std::pow(y - wither.y, 2) + std::pow(z - wither.z, 2) <= 400.0;
        };
        std::vector<std::int32_t> found;
        for (const ServerEntity* candidate : living) {
            if (candidate == &wither || candidate->removed || candidate->dying || candidate->health <= 0.0 ||
                !server::registry::entityAttributes::isLiving(candidate->type()) ||
                boss::isFriend(candidate->type().path()) ||
                !inBox(candidate->x, candidate->y, candidate->z, candidate->height())) {
                continue;
            }
            found.push_back(candidate->entityId());
        }
        for (const PlayerSession* player : watchersOf(world)) {
            if (targetable(*player) && inBox(player->x, player->y, player->z, 1.8)) {
                found.push_back(player->entityId());
            }
        }
        if (!found.empty()) {
            const std::int32_t pick = state.random.nextInt(static_cast<std::int32_t>(found.size()));
            state.targets[index] = found[static_cast<std::size_t>(pick)];
        }
    }
}

void WitherSystem::destroyBlocks(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                 ServerEntity& wither) {
    const std::int32_t width = mth::floor(static_cast<double>((static_cast<float>(wither.width()) / 2.0F) + 1.0F));
    const std::int32_t height = mth::floor(static_cast<double>(static_cast<float>(wither.height())));
    const std::int32_t bx = mth::floor(wither.x);
    const std::int32_t by = mth::floor(wither.y);
    const std::int32_t bz = mth::floor(wither.z);
    const std::int32_t air = server::registry::blocks::air().stateId;
    bool destroyed = false;
    // parity: `BlockPos.betweenClosed` walks x innermost and z outermost.
    for (std::int32_t z = bz - width; z <= bz + width; ++z) {
        for (std::int32_t y = by; y <= by + height; ++y) {
            for (std::int32_t x = bx - width; x <= bx + width; ++x) {
                const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
                if (!state.has_value() || server::registry::blocks::isAir(*state) ||
                    tagged(*state, "minecraft:wither_immune")) {
                    continue;
                }
                if (world.state().setBlockState(x, y, z, air)) {
                    destroyed = true;
                    broadcaster_->dropBrokenBlock(world, x, y, z, *state, true);
                    broadcaster_->announceRedstone(
                        world, watchers,
                        world::BlockChange{
                            .x = x,
                            .y = y,
                            .z = z,
                            .stateId = air,
                            .effects = {world::blockEffect::Effect{.type = kBlockDestroyedEvent, .data = *state}}});
                }
            }
        }
    }
    if (destroyed) {
        levelEvent(watchers, kBlockBreakEvent, wither);
    }
}

void WitherSystem::afterTurn(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                             const std::vector<ServerEntity*>& living, ServerEntity& wither) {
    boss::WitherState& state = stateOf(wither);
    if (wither.dying || wither.removed) {
        updateBar(watchers, wither, static_cast<float>(wither.health / boss::kMaxHealth));
        return;
    }
    const std::array<std::int32_t, 3> targetsBefore = state.targets;
    const std::int32_t invulnerableBefore = state.invulnerableTicks;
    float progress = 0.0F;
    if (state.invulnerableTicks > 0) {
        const std::int32_t left = state.invulnerableTicks - 1;
        progress = 1.0F - (static_cast<float>(left) / static_cast<float>(boss::kInvulnerableTicks));
        if (left <= 0) {
            explosions_->blowUp(
                world, watchers, wither.x, wither.y + eyeOf(wither), wither.z, kBirthExplosion, "wither",
                wither.entityId(),
                entity::mobOverrides::griefing(wither, world.gameRules().get(world::gameRules::kMobGriefing))
                    ? world::explosions::Spares{}
                    : world::explosions::Spares{[](auto&&...) { return true; }});
            if (!(wither.sourceNbt.has_value() && wither.sourceNbt->getBoolean("Silent"))) {
                // parity: `globalLevelEvent`, heard by every player on the server wherever they are.
                const play::LevelEventPacket event{
                    .event = kSpawnEvent,
                    .pos = play::BlockPos{mth::floor(wither.x), mth::floor(wither.y), mth::floor(wither.z)},
                    .data = 0,
                    .global = true};
                const double radius = world.settings().rules.tuning.witherSpawnSoundRadius;
                for (PlayerSession* player : players_->snapshot()) {
                    if (radius > 0.0 &&
                        (player->worldName != world.name() || std::abs(player->x - wither.x) > radius ||
                         std::abs(player->y - wither.y) > radius || std::abs(player->z - wither.z) > radius)) {
                        continue;
                    }
                    if (player->connection().sendPlayPacket) {
                        player->connection().sendPlayPacket(
                            play::LevelEventPacket::kId,
                            [&event](wire::io::ByteBuffer& buffer) { event.write(buffer); });
                    }
                }
            }
        }
        state.invulnerableTicks = left;
        if (wither.tickCount % 10 == 0) {
            heal(wither, kBirthHeal);
        }
    } else {
        tickHeads(world, living, wither, state);
        state.targets[0] = wither.targetId.value_or(0);
        if (state.destroyBlocksTick > 0) {
            state.destroyBlocksTick--;
            if (state.destroyBlocksTick == 0 &&
                entity::mobOverrides::griefing(wither, world.gameRules().get(world::gameRules::kMobGriefing))) {
                destroyBlocks(world, watchers, wither);
            }
        }
        if (wither.tickCount % 20 == 0) {
            heal(wither, 1.0);
        }
        progress = static_cast<float>(wither.health / boss::kMaxHealth);
    }
    for (std::size_t head = 0; head < state.targets.size(); ++head) {
        if (state.targets[head] != targetsBefore[head]) {
            syncData(watchers, wither, static_cast<std::uint8_t>(boss::kFirstTargetDataIndex + head),
                     state.targets[head]);
        }
    }
    if (state.invulnerableTicks != invulnerableBefore) {
        syncData(watchers, wither, boss::kInvulnerableDataIndex, state.invulnerableTicks);
    }
    updateBar(watchers, wither, progress);
}

void WitherSystem::keepBar(world::LoadedWorld& world, ServerEntity& wither) {
    updateBar(watchersOf(world), wither, static_cast<float>(wither.health / boss::kMaxHealth));
}

void WitherSystem::updateBar(const std::vector<PlayerSession*>& watchers, ServerEntity& wither, float progress) {
    std::set<std::int32_t> seenBy;
    if (!wither.removed && !(wither.dying && wither.health <= 0.0 && wither.deathTime >= 20)) {
        for (const PlayerSession* player : watchers) {
            const double dx = player->x - wither.x;
            const double dz = player->z - wither.z;
            if (std::abs(dx) <= kTrackingRange && std::abs(dz) <= kTrackingRange) {
                seenBy.insert(player->entityId());
            }
        }
    }
    const std::scoped_lock lock{barsLock_};
    Bar& bar = bars_[wither.entityId()];
    if (bar.view == nullptr) {
        play::BossEventPacket::Add style{.name = nameOf(wither),
                                         .progress = progress,
                                         .color = play::BossBarColor::Purple,
                                         .overlay = play::BossBarOverlay::Progress,
                                         .darkenScreen = true};
        bar.view = std::make_shared<BossBarView>(std::move(style));
    }
    bar.seen = true;
    bar.view->setProgress(*players_, progress);
    bar.view->track(*players_, seenBy);
}

bool WitherSystem::withBar(std::int32_t entityId,
                           const std::function<void(BossBarView&, player::PlayerRegistry&)>& use) {
    const std::scoped_lock lock{barsLock_};
    const auto found = bars_.find(entityId);
    if (found == bars_.end() || found->second.view == nullptr) {
        return false;
    }
    use(*found->second.view, *players_);
    return true;
}

void WitherSystem::sweepBars() {
    const std::scoped_lock lock{barsLock_};
    for (auto it = bars_.begin(); it != bars_.end();) {
        if (!it->second.seen) {
            it->second.view->clear(*players_);
            it = bars_.erase(it);
            continue;
        }
        it->second.seen = false;
        ++it;
    }
}

void WitherSystem::syncData(const std::vector<PlayerSession*>& watchers, const ServerEntity& wither, std::uint8_t index,
                            std::int32_t value) const {
    const play::SetEntityDataPacket packet{
        .entityId = wither.entityId(),
        .entries = {play::SetEntityDataPacket::Entry{.index = index,
                                                     .value = play::SetEntityDataPacket::IntValue{.value = value}}}};
    for (PlayerSession* player : watchers) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(play::SetEntityDataPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void WitherSystem::levelEvent(const std::vector<PlayerSession*>& watchers, std::int32_t event,
                              const ServerEntity& at) const {
    const std::int32_t x = mth::floor(at.x);
    const std::int32_t y = mth::floor(at.y);
    const std::int32_t z = mth::floor(at.z);
    for (PlayerSession* player : watchers) {
        const double dx = player->x - x;
        const double dy = player->y - y;
        const double dz = player->z - z;
        if ((dx * dx) + (dy * dy) + (dz * dz) <= kLevelEventRange * kLevelEventRange &&
            player->connection().sendLevelEvent) {
            player->connection().sendLevelEvent(event, x, y, z, 0);
        }
    }
}

void WitherSystem::skullLanded(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                               ServerEntity& skull, ServerEntity* mob, PlayerSession* player) {
    ServerEntity* owner = skull.ownerId.has_value() ? world.state().entity(*skull.ownerId) : nullptr;
    const bool ownedByLiving =
        owner != nullptr && !owner->removed && server::registry::entityAttributes::isLiving(owner->type());
    const float damage = ownedByLiving ? kSkullDamage : kUnownedSkullDamage;
    std::int32_t witherSeconds = 0;
    if (world.difficulty() == "normal") {
        witherSeconds = 10;
    } else if (world.difficulty() == "hard") {
        witherSeconds = 40;
    }
    if (mob != nullptr) {
        if (ownedByLiving && owner != nullptr && boss::isFriend(owner->type().path()) && mob->wither != nullptr) {
            mob->wither->nextHitFromFriend = true;
        }
        entity::entityCombat::blame(*mob, skull.ownerId, skull.entityId(), skull.type());
        const entity::entityCombat::Result result =
            entity::entityCombat::hurt(*mob, damage, skull.x, skull.z, {}, ownedByLiving ? "wither_skull" : "magic",
                                       owner != nullptr ? std::optional{owner->type().toString()} : std::nullopt);
        if (result.died && ownedByLiving && owner != nullptr) {
            heal(*owner, kKillHeal);
        } else if (result.landed && witherSeconds > 0 && options_.mobEffect) {
            static_cast<void>(options_.mobEffect(world, watchers, *mob,
                                                 world::ActiveEffect{.effect = world::mobEffects::wither(),
                                                                     .amplifier = kWitherEffectAmplifier,
                                                                     .remainingTicks = 20 * witherSeconds}));
        }
    }
    if (player != nullptr && player->connection().hurt) {
        player->connection().hurt(
            damage, ownedByLiving ? "witherSkull" : "magic",
            ownedByLiving ? world::damageTypes::kWitherSkull : world::damageTypes::kMagic,
            entity::DamageSource{.entityId = owner != nullptr ? owner->entityId() : 0,
                                 .x = skull.x,
                                 .z = skull.z,
                                 .name = owner != nullptr ? std::optional{nameOf(*owner)} : std::nullopt});
        if (player->dead && ownedByLiving && owner != nullptr) {
            heal(*owner, kKillHeal);
        } else if (witherSeconds > 0 && player->connection().addEffect) {
            player->noteEffectSource(skull.ownerId.value_or(skull.entityId()));
            player->connection().addEffect(world::mobEffects::wither(), 20 * witherSeconds, kWitherEffectAmplifier);
        }
    }
    const bool griefing = world.gameRules().get(world::gameRules::kMobGriefing);
    // parity: `WitherSkull.getBlockExplosionResistance`.
    // `0.8`: that is what lets a blue skull through obsidian.
    world::explosions::Resistance resistance;
    if (skull.dangerousSkull) {
        resistance = [this](std::int32_t stateId, float base) {
            return tagged(stateId, "minecraft:wither_immune") ? base : std::min(kDangerousResistance, base);
        };
    }
    explosions_->blowUp(world, watchers, skull.x, skull.y, skull.z, kSkullExplosion, "wither_skull", skull.entityId(),
                        griefing ? world::explosions::Spares{}
                                 : world::explosions::Spares{[](auto&&...) { return true; }},
                        resistance, skull.incendiary.value_or(false));
}

void WitherSystem::dropStar(world::LoadedWorld& world, const ServerEntity& wither) {
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    // parity: `Entity.spawnAtLocation` over `new ItemEntity`.
    static_cast<void>(
        world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                   .uuid = wire::common::Uuid::random(),
                                                   .item = wire::common::Identifier::vanilla("nether_star"),
                                                   .count = 1,
                                                   .x = wither.x,
                                                   .y = wither.y,
                                                   .z = wither.z,
                                                   .velocityX = (random.nextDouble() * 0.2) - 0.1,
                                                   .velocityY = 0.2,
                                                   .velocityZ = (random.nextDouble() * 0.2) - 0.1,
                                                   .pickupDelay = world::kSpawnedPickupDelay,
                                                   .age = world::kUnlimitedLifetimeAge}));
}

} // namespace basalt::server::runtime
