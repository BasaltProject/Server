#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cmath>
#include <string_view>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::protocol::play::Gamemode;
namespace brain = entity::wardenBrain;
namespace mth = wire::common::mth;

/// What a noise leads back to, as the warden sees it.
struct Somebody {
    std::int32_t id = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool player = false;
    bool spectator = false;
    bool living = true;
    /// parity: `Warden.canTargetEntity`.
    bool targetable = false;
    /// Who shot it, for a projectile. parity: `VibrationInfo.projectileOwner`.
    std::optional<std::int32_t> ownerId;
};

/// parity: `GameEventTags.WARDEN_CAN_LISTEN`: every vibration but a flap, the shriek, and a
/// sensor's click.
[[nodiscard]] bool listensTo(std::string_view event) {
    if (event == "shriek" || event == world::vibrations::kTendrilsClicking) {
        return true;
    }
    return event != "flap" && world::vibrations::frequencyOf(event) > 0;
}

/// parity: `AngerLevel.byAnger(getActiveAnger())`: the target's anger, or the highest of all.
[[nodiscard]] std::int32_t activeAnger(const ServerEntity& warden) {
    if (warden.targetId.has_value()) {
        return entity::wardens::angerAt(warden, *warden.targetId);
    }
    std::int32_t highest = 0;
    if (warden.anger.has_value()) {
        for (const auto& [suspect, anger] : *warden.anger) {
            highest = std::max(highest, anger);
        }
    }
    return highest;
}

/// parity: `AngerManagement.getActiveEntity`, the top suspect.
[[nodiscard]] std::optional<std::int32_t> activeEntity(const ServerEntity& warden) {
    std::optional<std::int32_t> top;
    std::int32_t highest = 0;
    if (warden.anger.has_value()) {
        for (const auto& [suspect, anger] : *warden.anger) {
            if (anger > highest) {
                highest = anger;
                top = suspect;
            }
        }
    }
    return top;
}

} // namespace

void MobActionSystem::wardenAngered(ServerEntity& warden, std::int32_t suspectId, std::int32_t amount, bool sound) {
    if (warden.noAi) {
        return;
    }
    world::LoadedWorld* world = worldOf(warden);
    bool suspectIsPlayer = false;
    if (world != nullptr) {
        for (const PlayerSession* player : watchersOf(*world)) {
            suspectIsPlayer = suspectIsPlayer || player->entityId() == suspectId;
        }
    }
    bool targetIsPlayer = false;
    if (warden.targetId.has_value() && world != nullptr) {
        for (const PlayerSession* player : watchersOf(*world)) {
            targetIsPlayer = targetIsPlayer || player->entityId() == *warden.targetId;
        }
    }
    const std::int32_t level = entity::wardens::anger(warden, suspectId, amount);
    // parity: a player it is now angry at takes over from a mob it was fighting.
    if (suspectIsPlayer && !targetIsPlayer && level >= entity::wardens::kAngry) {
        warden.targetId.reset();
    }
    if (sound && world != nullptr && brain::poseOf(warden) != wire::protocol::play::Pose::Roaring) {
        random::RandomSource random = freshRandom();
        const float pitch = ((random.nextFloat() - random.nextFloat()) * 0.2F) + 1.0F;
        // parity: `AngerLevel.getListeningSound`, calm below forty.
        playSound(*world, warden,
                  activeAnger(warden) >= 40 ? "entity.warden.listening_angry" : "entity.warden.listening", 10.0F,
                  pitch);
    }
}

void MobActionSystem::wardenHears(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                  ServerEntity& warden) {
    brain::WardenState& state = brain::stateOf(warden);
    if (state.vibrationCooldown > 0) {
        state.vibrationCooldown--;
    }
    if (state.recentProjectile > 0) {
        state.recentProjectile--;
    }
    const auto resolve = [&](const std::optional<wire::common::Uuid>& uuid) -> std::optional<Somebody> {
        if (!uuid.has_value()) {
            return std::nullopt;
        }
        for (const PlayerSession* player : watchers) {
            if (player->uuid() == *uuid) {
                return Somebody{.id = player->entityId(),
                                .x = player->x,
                                .y = player->y,
                                .z = player->z,
                                .player = true,
                                .spectator = player->gamemode == Gamemode::Spectator,
                                .targetable = !player->dead && (player->gamemode == Gamemode::Survival ||
                                                                player->gamemode == Gamemode::Adventure)};
            }
        }
        for (const ServerEntity* mob : world.state().entities()) {
            if (mob->uuid() == *uuid && !mob->removed) {
                const bool living = server::registry::entityAttributes::isLiving(mob->type());
                return Somebody{.id = mob->entityId(),
                                .x = mob->x,
                                .y = mob->y,
                                .z = mob->z,
                                .living = living,
                                .targetable = living && !mob->dying && !entity::wardens::isWarden(mob->type()) &&
                                              mob->type().path() != "armor_stand",
                                .ownerId = living ? std::nullopt : mob->ownerId};
            }
        }
        return std::nullopt;
    };
    const std::int64_t now = world.gameTime();
    const double eyeY = warden.y + entity::mobBreathing::eyeHeightOf(warden);
    std::int64_t lastTime = state.heardTime;
    std::size_t countAtLast = 0;
    for (const world::GameEventRecord& record : world.state().recentGameEvents()) {
        if (record.gameTime < state.heardTime) {
            continue;
        }
        const std::size_t seen = record.gameTime == lastTime ? ++countAtLast : (countAtLast = 1);
        lastTime = record.gameTime;
        if (record.gameTime == state.heardTime && seen <= state.heardCount) {
            continue;
        }
        // parity: `Listener.handleGameEvent`: deaf while one is on its way, and only to what it can
        // hear, from somebody it could fight, within sixteen blocks and past no wool.
        if (state.travelling.has_value() || !listensTo(record.event)) {
            continue;
        }
        const std::optional<Somebody> source = resolve(record.entity);
        if (source.has_value() && source->spectator) {
            continue;
        }
        if (warden.noAi || warden.dying || state.vibrationCooldown > 0 || brain::isDiggingOrEmerging(warden) ||
            (source.has_value() && source->living && !source->targetable)) {
            continue;
        }
        const double bx = mth::floor(record.x) - mth::floor(warden.x);
        const double by = mth::floor(record.y) - mth::floor(eyeY);
        const double bz = mth::floor(record.z) - mth::floor(warden.z);
        if ((bx * bx) + (by * by) + (bz * bz) > brain::kListenerRadius * brain::kListenerRadius ||
            world::vibrations::occludedBetween(world.state(), record.x, record.y, record.z, warden.x, eyeY, warden.z)) {
            continue;
        }
        const double dx = warden.x - record.x;
        const double dy = eyeY - record.y;
        const double dz = warden.z - record.z;
        brain::offer(state, brain::Heard{.event = record.event,
                                         .distance = static_cast<float>(std::sqrt((dx * dx) + (dy * dy) + (dz * dz))),
                                         .x = record.x,
                                         .y = record.y,
                                         .z = record.z,
                                         .entity = record.entity,
                                         .projectileOwner = record.projectileOwner,
                                         .tick = record.gameTime});
    }
    if (lastTime > state.heardTime || (lastTime == state.heardTime && countAtLast > state.heardCount)) {
        state.heardTime = lastTime;
        state.heardCount = countAtLast;
    }
    // parity: `VibrationSystem.Ticker.tick`: last tick's pick sets off, then travels a block a tick.
    if (!state.travelling.has_value() && state.candidate.has_value() && state.candidate->tick < now) {
        state.travelling = std::exchange(state.candidate, std::nullopt);
        state.travelTicks = mth::floor(static_cast<double>(state.travelling->distance));
    }
    if (!state.travelling.has_value()) {
        return;
    }
    state.travelTicks = std::max(0, state.travelTicks - 1);
    if (state.travelTicks > 0) {
        return;
    }
    const brain::Heard heard = *std::exchange(state.travelling, std::nullopt);
    if (warden.dying) {
        return;
    }
    // parity: `VibrationUser.onReceiveVibration`.
    state.vibrationCooldown = brain::kVibrationCooldown;
    entityEvent(warden, 61);
    {
        random::RandomSource random = freshRandom();
        const float pitch = ((random.nextFloat() - random.nextFloat()) * 0.2F) + 1.0F;
        playSound(world, warden, "entity.warden.tendril_clicks", 5.0F, pitch);
    }
    std::array<std::int32_t, 3> suspicious{mth::floor(heard.x), mth::floor(heard.y), mth::floor(heard.z)};
    const std::optional<Somebody> source = resolve(heard.entity);
    std::optional<Somebody> owner = resolve(heard.projectileOwner);
    // parity: `VibrationInfo` finds the owner of a projectile that made the noise itself.
    if (!owner.has_value() && source.has_value() && source->ownerId.has_value()) {
        for (const PlayerSession* player : watchers) {
            if (player->entityId() == *source->ownerId) {
                owner = resolve(player->uuid());
            }
        }
        if (const ServerEntity* shooter = world.state().entity(*source->ownerId);
            !owner.has_value() && shooter != nullptr) {
            owner = resolve(shooter->uuid());
        }
    }
    if (owner.has_value()) {
        const double ox = owner->x - warden.x;
        const double oy = owner->y - warden.y;
        const double oz = owner->z - warden.z;
        if ((ox * ox) + (oy * oy) + (oz * oz) < 30.0 * 30.0 && owner->targetable) {
            if (state.recentProjectile > 0) {
                suspicious = {mth::floor(owner->x), mth::floor(owner->y), mth::floor(owner->z)};
                wardenAngered(warden, owner->id, entity::wardens::kHeard, true);
            } else {
                wardenAngered(warden, owner->id, 10, true);
            }
        }
        state.recentProjectile = brain::kRecentProjectileTicks;
    } else if (source.has_value() && source->targetable) {
        wardenAngered(warden, source->id, entity::wardens::kHeard, true);
    }
    if (activeAnger(warden) < entity::wardens::kAngry) {
        const std::optional<std::int32_t> active = activeEntity(warden);
        if (owner.has_value() || !active.has_value() || (source.has_value() && *active == source->id)) {
            brain::setDisturbance(warden, suspicious);
        }
    }
}

} // namespace basalt::server::runtime
