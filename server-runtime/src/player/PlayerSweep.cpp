#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSweep.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <cmath>

namespace basalt::server::runtime::player {

namespace {

/// parity: `Player.attack`'s sweep: one point of damage scaled by how charged the swing was.
constexpr float kSweepDamage = 1.0F;
/// How far to each side of what was hit the sweep reaches, and how far above and below.
constexpr double kSweepRange = 1.0;
constexpr double kSweepHeight = 0.25;
/// And how far from the player itself, squared.
constexpr double kSweepPlayerRange = 9.0;
/// parity: `PanicGoal`, two seconds of running away.
constexpr std::int32_t kPanicTicks = 40;

} // namespace

std::vector<SweepHit> sweepAround(PlayerSession& session, world::LoadedWorld& world, const entity::ServerEntity& target,
                                  const float strength) {
    std::vector<SweepHit> caught;
    const float damage = kSweepDamage * strength;
    for (entity::ServerEntity* nearby : world.state().entities()) {
        if (nearby->entityId() == target.entityId() || nearby->removed ||
            !registry::entityAttributes::isLiving(nearby->type())) {
            continue;
        }
        if (std::abs(nearby->x - target.x) > kSweepRange + (nearby->width() / 2.0) ||
            std::abs(nearby->z - target.z) > kSweepRange + (nearby->width() / 2.0)) {
            continue;
        }
        if (nearby->y > target.y + target.height() + kSweepHeight ||
            nearby->y + nearby->height() < target.y - kSweepHeight) {
            continue;
        }
        const double dx = nearby->x - session.x;
        const double dz = nearby->z - session.z;
        if ((dx * dx) + (dz * dz) >= kSweepPlayerRange) {
            continue;
        }
        entity::entityCombat::blame(*nearby, session.entityId(), session.entityId(),
                                    wire::common::Identifier::vanilla("player"));
        const entity::entityCombat::Result result =
            entity::entityCombat::hurt(*nearby, damage, session.x, session.z, {}, "sweep", session.name());
        if (!result.landed) {
            continue;
        }
        entity::entityCombat::rememberPlayer(*nearby, session.uuid());
        world::vibrations::emit(world.state(), "entity_damage", nearby->x, nearby->y, nearby->z, world.gameTime());
        if (!result.died) {
            nearby->hurtBy = session.entityId();
            // parity: `Warden.hurtServer`, which adds the angry threshold plus twenty to whoever hit it.
            if (entity::wardens::isWarden(nearby->type())) {
                entity::wardens::anger(*nearby, session.entityId(), entity::wardens::kHurt);
                MobActionSystem::wardenStruck(*nearby, session.entityId(), true, 0.0);
            }
            nearby->panicTicks = kPanicTicks;
        }
        caught.push_back(SweepHit{.entityId = nearby->entityId(), .died = result.died});
    }
    return caught;
}

} // namespace basalt::server::runtime::player
