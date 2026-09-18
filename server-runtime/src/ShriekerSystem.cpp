#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/ShriekerSystem.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/SculkShriekers.hpp>

#include <cmath>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using wire::protocol::play::Gamemode;
namespace shriekers = world::sculkShriekers;

/// parity: `SOUND_BY_LEVEL`, the warden's reply at each warning.
[[nodiscard]] std::string_view replyFor(std::int32_t level) {
    switch (level) {
    case 1:
        return "entity.warden.nearby_close";
    case 2:
        return "entity.warden.nearby_closer";
    case 3:
        return "entity.warden.nearby_closest";
    case 4:
        return "entity.warden.listening_angry";
    default:
        return {};
    }
}

} // namespace

ShriekerSystem::ShriekerSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster,
                               NaturalSpawningSystem& spawning, MobActionSystem& mobs, Options options)
    : players_(&players), broadcaster_(&broadcaster), spawning_(&spawning), mobs_(&mobs), options_(std::move(options)) {
}

std::vector<PlayerSession*> ShriekerSystem::watchersOf(const world::LoadedWorld& world) const {
    std::vector<PlayerSession*> watchers;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    return watchers;
}

void ShriekerSystem::steppedOn(world::LoadedWorld& world, PlayerSession& player, std::int32_t x, std::int32_t y,
                               std::int32_t z) {
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    if (state.has_value() && shriekers::isShrieker(*state)) {
        tryShriek(world, player, x, y, z);
    }
}

bool ShriekerSystem::canRespond(const world::LoadedWorld& world, std::int32_t stateId) const {
    return shriekers::canSummon(stateId) && world.difficulty() != "peaceful" &&
           world.gameRules().get(world::gameRules::kSpawnWardens);
}

void ShriekerSystem::tryShriek(world::LoadedWorld& world, PlayerSession& player, std::int32_t x, std::int32_t y,
                               std::int32_t z) {
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    if (!state.has_value() || !shriekers::isShrieker(*state) || shriekers::isShrieking(*state) || player.dead) {
        return;
    }
    shriekers::setWarningLevel(world.state(), x, y, z, 0);
    if (!canRespond(world, *state) || tryToWarn(world, player, x, y, z)) {
        shriek(world, player, x, y, z);
    }
}

bool ShriekerSystem::tryToWarn(world::LoadedWorld& world, PlayerSession& player, std::int32_t x, std::int32_t y,
                               std::int32_t z) {
    const double cx = x + 0.5;
    const double cy = y + 0.5;
    const double cz = z + 0.5;
    // parity: `hasNearbyWarden`, a 48-block box round the shrieker.
    for (const entity::ServerEntity* candidate : world.state().entities()) {
        if (!candidate->removed && entity::wardens::isWarden(candidate->type()) &&
            std::abs(candidate->x - cx) < shriekers::kWardenCheckHalf + (candidate->width() / 2.0) &&
            candidate->y < cy + shriekers::kWardenCheckHalf &&
            candidate->y + candidate->height() > cy - shriekers::kWardenCheckHalf &&
            std::abs(candidate->z - cz) < shriekers::kWardenCheckHalf + (candidate->width() / 2.0)) {
            return false;
        }
    }
    std::vector<world::wardenSpawnTracker::Tracker*> trackers;
    bool triggerFound = false;
    for (PlayerSession* other : watchersOf(world)) {
        const double dx = other->x - cx;
        const double dy = other->y - cy;
        const double dz = other->z - cz;
        if (other->dead || other->gamemode == Gamemode::Spectator ||
            (dx * dx) + (dy * dy) + (dz * dz) >= shriekers::kPlayerSearchRadius * shriekers::kPlayerSearchRadius) {
            continue;
        }
        trackers.push_back(&other->wardenTracker);
        triggerFound = triggerFound || other == &player;
    }
    if (!triggerFound) {
        trackers.push_back(&player.wardenTracker);
    }
    const std::optional<std::int32_t> level = world::wardenSpawnTracker::warn(trackers);
    if (!level.has_value()) {
        return false;
    }
    shriekers::setWarningLevel(world.state(), x, y, z, *level);
    return true;
}

void ShriekerSystem::shriek(world::LoadedWorld& world, const PlayerSession& player, std::int32_t x, std::int32_t y,
                            std::int32_t z) {
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    const std::optional<std::int32_t> loud = state.has_value() ? shriekers::withShrieking(*state, true) : std::nullopt;
    if (!loud.has_value() || !world.state().setBlockState(x, y, z, *loud)) {
        return;
    }
    // parity: flag 2: the clients see it and nothing around it is told.
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    broadcaster_->announceBlock(
        watchers, world::BlockChange{.x = x,
                                     .y = y,
                                     .z = z,
                                     .stateId = *loud,
                                     .effects = {world::blockEffect::Effect{.type = shriekers::kShriekEvent}}});
    world.state().scheduleBlockTick(x, y, z, wire::common::Identifier::vanilla("sculk_shrieker"),
                                    shriekers::kShriekTicks);
    // parity: `GameEvent.SHRIEK`, which no sensor weighs but a warden in earshot hears as the player.
    for (entity::ServerEntity* candidate : world.state().entities()) {
        if (candidate->removed || !entity::wardens::isWarden(candidate->type())) {
            continue;
        }
        const double dx = candidate->x - (x + 0.5);
        const double dy = candidate->y - (y + 0.5);
        const double dz = candidate->z - (z + 0.5);
        if ((dx * dx) + (dy * dy) + (dz * dz) <= entity::wardens::kHearingRange * entity::wardens::kHearingRange) {
            static_cast<void>(entity::wardens::anger(*candidate, player.entityId(), entity::wardens::kHeard));
        }
    }
}

void ShriekerSystem::scheduledTick(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                   std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    if (!state.has_value() || !shriekers::isShrieking(*state)) {
        return;
    }
    const std::optional<std::int32_t> quiet = shriekers::withShrieking(*state, false);
    if (!quiet.has_value() || !world.state().setBlockState(x, y, z, *quiet)) {
        return;
    }
    broadcaster_->announceRedstone(world, watchers, world::BlockChange{x, y, z, *quiet, {}});
    tryRespond(world, x, y, z);
}

void ShriekerSystem::tryRespond(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    const std::int32_t level = shriekers::warningLevel(world.state(), x, y, z);
    if (!state.has_value() || !canRespond(world, *state) || level <= 0) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    if (!trySummonWarden(world, x, y, z)) {
        const std::string_view reply = replyFor(level);
        if (!reply.empty()) {
            random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
            const std::int32_t sx =
                x + random.nextIntBetweenInclusive(-shriekers::kReplyScatter, shriekers::kReplyScatter);
            const std::int32_t sy =
                y + random.nextIntBetweenInclusive(-shriekers::kReplyScatter, shriekers::kReplyScatter);
            const std::int32_t sz =
                z + random.nextIntBetweenInclusive(-shriekers::kReplyScatter, shriekers::kReplyScatter);
            broadcaster_->sendEffects(watchers, sx, sy, sz, 0,
                                      {world::blockEffect::Sound{.name = std::string{reply},
                                                                 .source = wire::protocol::play::SoundSource::Hostile,
                                                                 .volume = 5.0F,
                                                                 .pitch = 1.0F}});
        }
    }
    // parity: `Warden.applyDarknessAround` from the middle of the shrieker, forty blocks out.
    const double cx = x + 0.5;
    const double cy = y + 0.5;
    const double cz = z + 0.5;
    for (PlayerSession* player : watchers) {
        const double dx = player->x - cx;
        const double dy = player->y - cy;
        const double dz = player->z - cz;
        const bool survival = player->gamemode == Gamemode::Survival || player->gamemode == Gamemode::Adventure;
        if (player->dead || !survival ||
            (dx * dx) + (dy * dy) + (dz * dz) >= shriekers::kDarknessRadius * shriekers::kDarknessRadius) {
            continue;
        }
        const auto held = player->effects.find(world::mobEffects::darkness());
        const bool renew =
            held == player->effects.end() || (!held->second.infinite() && held->second.remainingTicks <= 199);
        if (renew && player->connection().addEffect) {
            player->connection().addEffect(world::mobEffects::darkness(), 260, 0);
        }
    }
}

bool ShriekerSystem::trySummonWarden(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (shriekers::warningLevel(world.state(), x, y, z) < shriekers::kMaxWarning) {
        return false;
    }
    const wire::common::Identifier warden = entity::wardens::type();
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    std::int32_t attempt = 0;
    while (attempt < shriekers::kSpawnAttempts) {
        const std::optional<std::array<std::int32_t, 3>> at =
            shriekers::spawnPosition(world.state(), x, y, z, random, attempt);
        if (!at.has_value()) {
            return false;
        }
        // parity: `checkSpawnObstruction`, nothing already standing where it would rise.
        const entity::Aabb box = entity::Aabb::of((*at)[0] + 0.5, static_cast<double>((*at)[1]), (*at)[2] + 0.5,
                                                  server::registry::entityTypeShapes::widthOf(warden),
                                                  server::registry::entityTypeShapes::heightOf(warden));
        bool obstructed = false;
        for (const entity::ServerEntity* other : world.state().entities()) {
            if (other->removed || !server::registry::entityAttributes::isLiving(other->type())) {
                continue;
            }
            const entity::Aabb theirs = entity::Aabb::of(other->x, other->y, other->z, other->width(), other->height());
            obstructed = obstructed || box.intersects(theirs);
        }
        if (obstructed) {
            continue;
        }
        entity::ServerEntity* spawned = spawning_->spawnMob(
            world, warden, (*at)[0] + 0.5, static_cast<double>((*at)[1]), (*at)[2] + 0.5, 0.0F, "triggered");
        if (spawned != nullptr) {
            mobs_->wardenEmerges(world, *spawned);
            return true;
        }
    }
    return false;
}

void ShriekerSystem::tickTrackers(const world::LoadedWorld& world) {
    for (PlayerSession* player : watchersOf(world)) {
        if (!player->dead) {
            player->wardenTracker.tick();
        }
    }
}

} // namespace basalt::server::runtime
