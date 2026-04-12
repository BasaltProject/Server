#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <string_view>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;

namespace mth = wire::common::mth;

/// parity: `LlamaSpit.onHitEntity`'s single point, and `DamageTypes.SPIT`'s message id.
constexpr float kSpitDamage = 1.0F;
constexpr std::string_view kDeathSpit = "mob";

/// parity: `PanicGoal`, which runs for as long as the last hit is remembered.
constexpr std::int32_t kPanicTicks = 40;

/// parity: `ProjectileUtil.computeMargin`, which grows to three tenths over the first few ticks.
constexpr float kMaxMargin = 0.3F;
constexpr float kMarginGrowth = 20.0F;

/// parity: `Entity.updateFluidHeightAndDoFluidPushing`, which shrinks the box a hair first.
constexpr double kFluidShrink = 0.001;

/// parity: `Projectile.updateRotation`: float degrees widened, and a fifth of the way a tick.
constexpr double kDegrees = static_cast<double>(180.0F / static_cast<float>(std::numbers::pi));
constexpr float kTurnShare = 0.2F;

/// parity: `BlockStateBase.isAir`, the three airs. A square that is not loaded counts as one.
[[nodiscard]] bool isAir(const std::optional<std::int32_t>& state) {
    if (!state.has_value()) {
        return true;
    }
    const Identifier* block = server::registry::blockStateDefinitions::blockOf(*state);
    return block == nullptr || block->path() == "air" || block->path() == "cave_air" || block->path() == "void_air";
}

/// parity: `findBlocksIn(getBoundingBox()).filterState(isAir).noneMatched()`: every square the box
/// touches is something other than air.
[[nodiscard]] bool buried(world::WorldState& world, const ServerEntity& spit) {
    const double half = spit.width() / 2.0;
    for (std::int32_t x = mth::floor(spit.x - half); x <= mth::floor(spit.x + half); ++x) {
        for (std::int32_t y = mth::floor(spit.y); y <= mth::floor(spit.y + spit.height()); ++y) {
            for (std::int32_t z = mth::floor(spit.z - half); z <= mth::floor(spit.z + half); ++z) {
                if (isAir(world.blockState(x, y, z))) {
                    return false;
                }
            }
        }
    }
    return true;
}

/// parity: `Entity.isInWater`, water anywhere in the box that reaches above its bottom.
[[nodiscard]] bool touchingWater(world::WorldState& world, const ServerEntity& spit) {
    const double half = (spit.width() / 2.0) - kFluidShrink;
    const double minY = spit.y + kFluidShrink;
    const double maxY = spit.y + spit.height() - kFluidShrink;
    const auto ceiling = [](double value) { return static_cast<std::int32_t>(std::ceil(value)); };
    for (std::int32_t x = mth::floor(spit.x - half); x < ceiling(spit.x + half); ++x) {
        for (std::int32_t y = mth::floor(minY); y < ceiling(maxY); ++y) {
            for (std::int32_t z = mth::floor(spit.z - half); z < ceiling(spit.z + half); ++z) {
                const std::optional<std::int32_t> state = world.blockState(x, y, z);
                if (!state.has_value() || world::fluidFlow::kindOf(*state) != world::FluidKind::Water) {
                    continue;
                }
                const float height = static_cast<float>(y) + world::fluidFlow::heightAt(world, x, y, z);
                if (static_cast<double>(height) >= minY) {
                    return true;
                }
            }
        }
    }
    return false;
}

/// parity: `Projectile.lerpRotation`, the short way round.
[[nodiscard]] float lerpRotation(float from, float to) {
    while (to - from < -180.0F) {
        from -= 360.0F;
    }
    while (to - from >= 180.0F) {
        from += 360.0F;
    }
    return from + (kTurnShare * (to - from));
}

} // namespace

void CombatSystem::tickLlamaSpit(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                 const std::vector<ServerEntity*>& living, ServerEntity& spit,
                                 const entity::projectiles::Kind& kind) {
    ++spit.tickCount;
    // parity: `Projectile.tick` runs first here, so the shot is heard from where it starts.
    if (!spit.announced) {
        spit.announced = true;
        emitVibration(world, "projectile_shoot", spit.x, spit.y, spit.z, spit.uuid());
    }
    const bool wet = touchingWater(world.state(), spit);
    const float pitchBefore = spit.pitch;
    const float yawBefore = spit.yaw;
    const double fromX = spit.x;
    const double fromY = spit.y;
    const double fromZ = spit.z;
    const double moveX = spit.velocityX;
    const double moveY = spit.velocityY;
    const double moveZ = spit.velocityZ;
    const std::optional<entity::BlockHit> hit =
        entity::blockRay::cast(world.state(), fromX, fromY, fromZ, fromX + moveX, fromY + moveY, fromZ + moveZ);
    // parity: `ProjectileUtil.getHitResult` looks for somebody only as far as the block let it go.
    spit.x = hit.has_value() ? hit->x : fromX + moveX;
    spit.y = hit.has_value() ? hit->y : fromY + moveY;
    spit.z = hit.has_value() ? hit->z : fromZ + moveZ;
    const float margin = std::max(0.0F, std::min(kMaxMargin, static_cast<float>(spit.tickCount - 2) / kMarginGrowth));
    const bool turned = deflected(watchers, living, spit, fromX, fromY, fromZ);
    std::optional<Target> target;
    if (!turned) {
        target = targetAlong(watchers, living, spit, fromX, fromY, fromZ, static_cast<double>(margin));
    }
    spit.x = fromX;
    spit.y = fromY;
    spit.z = fromZ;
    // parity: `LlamaSpit.onHitEntity` leaves it flying; only a block puts it out.
    if (target.has_value()) {
        spitHits(world, watchers, living, spit, *target, fromX, fromZ);
    } else if (!turned && hit.has_value()) {
        hitBlock(world, watchers, spit, *hit, false);
        spit.removed = true;
        return;
    }
    const double horizontal = std::sqrt((spit.velocityX * spit.velocityX) + (spit.velocityZ * spit.velocityZ));
    spit.pitch = lerpRotation(pitchBefore, static_cast<float>(std::atan2(spit.velocityY, horizontal) * kDegrees));
    spit.yaw = lerpRotation(yawBefore, static_cast<float>(std::atan2(spit.velocityX, spit.velocityZ) * kDegrees));
    // parity: gone once every square its box touches is solid, or in water, both judged where it
    // stood when the tick began.
    if (buried(world.state(), spit) || wet) {
        spit.removed = true;
        return;
    }
    // parity: the drag is taken off the movement it started with, so a deflection's reversal is
    // lost again.
    spit.velocityX = moveX * kind.drag;
    spit.velocityY = moveY * kind.drag;
    spit.velocityZ = moveZ * kind.drag;
    if (!spit.noGravity) {
        spit.velocityY -= kind.gravity;
    }
    spit.x = fromX + moveX;
    spit.y = fromY + moveY;
    spit.z = fromZ + moveZ;
}

void CombatSystem::spitHits(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                            const std::vector<ServerEntity*>& living, const ServerEntity& spit, const Target& target,
                            double fromX, double fromZ) {
    // parity: `getOwner() instanceof LivingEntity`: a spit whose llama is gone hurts nobody.
    const bool owned = spit.ownerId.has_value() && std::ranges::any_of(living, [&spit](const ServerEntity* one) {
                           return one->entityId() == *spit.ownerId && !one->removed;
                       });
    if (target.mob != nullptr) {
        ServerEntity& mob = *target.mob;
        if (owned && world::endCrystals::isCrystal(mob.type())) {
            hitCrystal(world, watchers, mob, false);
        } else if (owned) {
            if (mob.dragon != nullptr) {
                mob.dragon->hitPart = target.dragonPart;
            }
            entity::entityCombat::blame(mob, spit.ownerId, spit.entityId(), spit.type());
            const entity::entityCombat::Result result = entity::entityCombat::hurt(
                mob, kSpitDamage, fromX, fromZ, hooks_, world::damageTypes::kSpit.path(), std::nullopt, false);
            if (result.landed) {
                mob.hurtBy = spit.ownerId;
                mob.panicTicks = kPanicTicks;
                announceEntityHit(watchers, mob, world::damageTypes::kSpit, spit.entityId(), result.died);
                emitVibration(world, "entity_damage", mob.x, mob.y, mob.z, mob.uuid());
            }
        }
        // parity: `Projectile.onHit` raises `PROJECTILE_LAND` after the entity is dealt with.
        emitVibration(world, "projectile_land", mob.x, mob.y, mob.z, spit.uuid());
        return;
    }
    if (target.player == nullptr) {
        return;
    }
    if (owned && target.player->connection().hurt) {
        target.player->connection().hurt(kSpitDamage, kDeathSpit, world::damageTypes::kSpit,
                                         entity::DamageSource{.entityId = spit.entityId(),
                                                              .x = fromX,
                                                              .z = fromZ,
                                                              .name = shooterName(watchers, living, spit),
                                                              .ownerId = spit.ownerId});
    }
    emitVibration(world, "projectile_land", target.player->x, target.player->y, target.player->z, spit.uuid());
}

} // namespace basalt::server::runtime
