#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cmath>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
namespace brain = entity::wardenBrain;
using wire::protocol::play::Gamemode;

/// parity: `SonicBoom`'s push, half a block up and two and a half along.
constexpr double kPushVertical = 0.5;
constexpr double kPushHorizontal = 2.5;
/// parity: `EntityAttachment.WARDEN_CHEST`, where the boom leaves from.
constexpr double kChestHeight = 1.6;
constexpr double kEyeShare = 0.85;
constexpr double kPlayerEye = 1.62;
constexpr std::int32_t kAngerSyncInterval = 20;

[[nodiscard]] bool survival(const PlayerSession& player) {
    return player.gamemode == Gamemode::Survival || player.gamemode == Gamemode::Adventure;
}

} // namespace

void MobActionSystem::wardenRoar(ServerEntity& warden, std::int32_t targetId) {
    world::LoadedWorld* world = worldOf(warden);
    if (world != nullptr) {
        runWardenActions(*world, watchersOf(*world), warden, brain::startRoar(warden, targetId));
    }
}

void MobActionSystem::wardenEmerges(world::LoadedWorld& world, ServerEntity& warden) {
    runWardenActions(world, watchersOf(world), warden, brain::startEmerging(warden));
}

void MobActionSystem::runWardenActions(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                       ServerEntity& warden, const std::vector<brain::Action>& actions) {
    for (const brain::Action& action : actions) {
        switch (action.kind) {
        case brain::Action::Kind::Sound:
            playSound(world, warden, action.sound, action.volume);
            break;
        case brain::Action::Kind::EntityEvent:
            for (PlayerSession* watcher : watchers) {
                if (watcher->connection().sendPlayPacket) {
                    watcher->connection().sendPlayPacket(wire::protocol::play::EntityEventPacket::kId,
                                                         [&](wire::io::ByteBuffer& buffer) {
                                                             wire::protocol::play::EntityEventPacket{
                                                                 .entityId = warden.entityId(), .eventId = action.event}
                                                                 .write(buffer);
                                                         });
                }
            }
            break;
        case brain::Action::Kind::Pose:
            // The pose travels with the entity's live metadata.
            break;
        case brain::Action::Kind::Discard:
            warden.removed = true;
            break;
        case brain::Action::Kind::StartFight:
            warden.targetId = action.targetId;
            break;
        case brain::Action::Kind::SonicBlast:
            sonicBlast(world, watchers, warden);
            break;
        }
    }
}

void MobActionSystem::sonicBlast(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                 ServerEntity& warden) {
    if (!warden.targetId.has_value()) {
        return;
    }
    const std::int32_t targetId = *warden.targetId;
    PlayerSession* player = nullptr;
    for (PlayerSession* candidate : watchers) {
        if (candidate->entityId() == targetId) {
            player = candidate;
        }
    }
    ServerEntity* mob = player == nullptr ? world.state().entity(targetId) : nullptr;
    if ((player == nullptr || player->dead || !survival(*player)) && (mob == nullptr || mob->removed || mob->dying)) {
        return;
    }
    const double eyeX = player != nullptr ? player->x : mob->x;
    const double eyeY = player != nullptr ? player->y + kPlayerEye : mob->y + (mob->height() * kEyeShare);
    const double eyeZ = player != nullptr ? player->z : mob->z;
    const double fromX = warden.x;
    const double fromY = warden.y + kChestHeight;
    const double fromZ = warden.z;
    const double dx = eyeX - fromX;
    const double dy = eyeY - fromY;
    const double dz = eyeZ - fromZ;
    const double length = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    const double nx = length < 1.0e-4 ? 0.0 : dx / length;
    const double ny = length < 1.0e-4 ? 0.0 : dy / length;
    const double nz = length < 1.0e-4 ? 0.0 : dz / length;
    // parity: one `sonic_boom` particle a block from the chest to seven past the target.
    const std::int32_t steps = wire::common::mth::floor(length) + 7;
    for (PlayerSession* watcher : watchers) {
        if (!watcher->connection().showParticle) {
            continue;
        }
        for (std::int32_t step = 1; step < steps; ++step) {
            watcher->connection().showParticle("sonic_boom", fromX + (nx * step), fromY + (ny * step),
                                               fromZ + (nz * step));
        }
    }
    playSound(world, warden, "entity.warden.sonic_boom", 3.0F);
    if (player != nullptr) {
        if (player->connection().hurt) {
            player->connection().hurt(brain::kSonicDamage, "sonic_boom", world::damageTypes::kSonicBoom,
                                      entity::DamageSource{.entityId = warden.entityId(),
                                                           .x = warden.x,
                                                           .z = warden.z,
                                                           .name = entity::entityCombat::displayName(warden)});
        }
        if (player->connection().pushBack) {
            player->connection().pushBack(nx * kPushHorizontal, ny * kPushVertical, nz * kPushHorizontal, warden.yaw);
        }
        return;
    }
    entity::entityCombat::blame(*mob, warden.entityId(), warden.entityId(), warden.type());
    const entity::entityCombat::Result result = entity::entityCombat::hurt(
        *mob, brain::kSonicDamage, warden.x, warden.z, hooks_.combat, "sonic_boom", warden.type().toString());
    if (!result.landed) {
        return;
    }
    const double resistance = 1.0 - mob->attributeWithModifiers("minecraft:knockback_resistance");
    mob->velocityX += nx * kPushHorizontal * resistance;
    mob->velocityY += ny * kPushVertical * resistance;
    mob->velocityZ += nz * kPushHorizontal * resistance;
    combat_->announceEntityHit(watchers, *mob, world::damageTypes::kSonicBoom, warden.entityId(), result.died);
}

bool MobActionSystem::wardenSwings(world::LoadedWorld& world, ServerEntity& mob) {
    if (mob.warden == nullptr) {
        return true;
    }
    // parity: `ATTACK_COOLING_DOWN`, which a sonic boom holds for its whole sixty ticks.
    if (mob.warden->sonicTicks > 0) {
        return false;
    }
    runWardenActions(world, watchersOf(world), mob, brain::meleeHit(mob));
    return true;
}

void MobActionSystem::wardenStruck(ServerEntity& warden, std::int32_t attackerId, bool direct, double distanceSquared) {
    // parity: `Warden.hurtServer`: a direct hit, or one from within five blocks, is fought at once.
    if (warden.noAi || brain::isDiggingOrEmerging(warden) || warden.targetId.has_value() ||
        (!direct && distanceSquared >= 25.0)) {
        return;
    }
    brain::WardenState& state = brain::stateOf(warden);
    state.roarTicks = 0;
    state.roarTarget.reset();
    state.sonicCooldown = brain::kFirstSonicCooldown;
    warden.targetId = attackerId;
}

void MobActionSystem::tickWarden(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                 ServerEntity& warden) {
    // parity: `AngerManagement.tick`, once a second.
    if (warden.tickCount % kAngerSyncInterval == 0) {
        entity::wardens::tick(warden);
    }
    if (warden.touchCooldown > 0) {
        warden.touchCooldown--;
    }
    // parity: `Warden.tick` runs its vibration ticker first.
    wardenHears(world, watchers, warden);
    if (warden.noAi) {
        return;
    }
    brain::View view{.onGround = warden.onGround,
                     .inFluid = entity::ai::inWater(world.state(), warden),
                     .persistent = warden.persistent || warden.customName.has_value()};
    if (warden.targetId.has_value()) {
        for (const PlayerSession* player : watchers) {
            if (player->entityId() == *warden.targetId) {
                view.target = std::array<double, 3>{player->x, player->y, player->z};
            }
        }
        if (!view.target.has_value()) {
            if (const ServerEntity* mob = world.state().entity(*warden.targetId); mob != nullptr && !mob->removed) {
                view.target = std::array<double, 3>{mob->x, mob->y, mob->z};
            }
        }
    }
    runWardenActions(world, watchers, warden, brain::tick(warden, view));
    // parity: `Warden.customServerAiStep`'s darkness, every six seconds offset by its own id.
    if ((warden.tickCount + warden.entityId()) % brain::kDarknessInterval != 0) {
        return;
    }
    for (PlayerSession* player : watchers) {
        const double dx = player->x - warden.x;
        const double dy = player->y - warden.y;
        const double dz = player->z - warden.z;
        if (player->dead || !survival(*player) ||
            (dx * dx) + (dy * dy) + (dz * dz) >= brain::kDarknessRadius * brain::kDarknessRadius) {
            continue;
        }
        const auto held = player->effects.find(world::mobEffects::darkness());
        const bool renew =
            held == player->effects.end() ||
            (!held->second.infinite() && held->second.remainingTicks <= brain::kDarknessDisplayLimit - 1);
        if (renew && player->connection().addEffect) {
            player->noteEffectSource(warden.entityId());
            player->connection().addEffect(world::mobEffects::darkness(), brain::kDarknessTicks, 0);
        }
    }
}

} // namespace basalt::server::runtime
