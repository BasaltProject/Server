#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <array>
#include <string_view>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;

/// parity: `LargeFireball.onHitEntity`, six points before the blast.
constexpr float kLargeFireballDamage = 6.0F;
constexpr std::string_view kDeathFireball = "fireball";
constexpr std::string_view kDeathOnFire = "onFire";

/// parity: `#minecraft:redirectable_projectile`.
constexpr std::array<std::string_view, 3> kRedirectable{"fireball", "wind_charge", "breeze_wind_charge"};

} // namespace

bool CombatSystem::isRedirectable(const Identifier& type) {
    return type.namespaceName() == Identifier::kVanillaNamespace &&
           std::ranges::find(kRedirectable, type.path()) != kRedirectable.end();
}

void CombatSystem::deflectByPunch(const std::vector<PlayerSession*>& watchers, ServerEntity& projectile,
                                  const PlayerSession& by) {
    // parity: `ProjectileDeflection.AIM_DEFLECT` at power one.
    const entity::enderDragon::Vec3 look = entity::enderDragon::viewVector(by.pitch, by.yaw);
    projectile.velocityX = look.x;
    projectile.velocityY = look.y;
    projectile.velocityZ = look.z;
    projectile.ownerId = by.entityId();
    for (PlayerSession* watcher : watchers) {
        if (!watcher->connection().sendPlayPacket) {
            continue;
        }
        watcher->connection().sendPlayPacket(
            wire::protocol::play::SetEntityMotionPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                wire::protocol::play::SetEntityMotionPacket{.entityId = projectile.entityId(),
                                                            .velocityX = projectile.velocityX,
                                                            .velocityY = projectile.velocityY,
                                                            .velocityZ = projectile.velocityZ}
                    .write(buffer);
            });
    }
}

void CombatSystem::hitByLargeFireball(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                      const std::vector<ServerEntity*>& living, ServerEntity& fireball,
                                      const Target& target, double fromX, double fromZ) {
    // parity: `DamageSources.fireball`.
    const bool attributed = fireball.ownerId.has_value();
    const Identifier type = Identifier::vanilla(attributed ? "fireball" : "unattributed_fireball");
    if (hitVehicle(world, target.mob, kLargeFireballDamage, type, fireball)) {
        return;
    }
    if (target.mob != nullptr) {
        ServerEntity& mob = *target.mob;
        if (mob.dragon != nullptr) {
            mob.dragon->hitPart = target.dragonPart;
        }
        markWitherFriend(world, mob, fireball.ownerId);
        entity::entityCombat::blame(mob, fireball.ownerId, fireball.entityId(), fireball.type());
        const entity::entityCombat::Result result =
            entity::entityCombat::hurt(mob, kLargeFireballDamage, fromX, fromZ, hooks_, type.path(), std::nullopt,
                                       firedByPlayer(watchers, fireball));
        if (!result.landed) {
            return;
        }
        rememberShooter(watchers, fireball, mob);
        if (fireball.ownerId.has_value()) {
            mob.hurtBy = *fireball.ownerId;
        }
        announceEntityHit(watchers, mob, type, fireball.entityId(), result.died);
        emitVibration(world, "entity_damage", mob.x, mob.y, mob.z, mob.uuid());
        return;
    }
    if (target.player != nullptr && target.player->connection().hurt) {
        target.player->connection().hurt(kLargeFireballDamage, attributed ? kDeathFireball : kDeathOnFire, type,
                                         entity::DamageSource{.entityId = fireball.entityId(),
                                                              .x = fromX,
                                                              .z = fromZ,
                                                              .name = shooterName(watchers, living, fireball),
                                                              .ownerId = fireball.ownerId});
    }
}

} // namespace basalt::server::runtime
