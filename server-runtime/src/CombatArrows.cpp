#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PotionEffects.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <string_view>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;

/// parity: `death.attack.arrow`, and `AbstractArrow`'s own two points of damage.
constexpr std::string_view kDeathArrow = "arrow";
constexpr double kArrowBaseDamage = 2.0;

/// parity: `AbstractArrow.onHitEntity`'s own `scale(0.2)` when the hit did not land.
constexpr double kArrowBounce = 0.2;

/// parity: `PanicGoal`, which runs for as long as the last hit is remembered.
constexpr std::int32_t kPanicTicks = 40;

/// parity: `SpectralArrow.duration`, ten seconds of glowing.
constexpr std::int32_t kGlowingTicks = 200;
/// parity: the tipped arrow's own `potion_duration_scale`, an eighth of the potion.
constexpr float kTippedScale = 0.125F;
/// parity: `HealOrHarmMobEffect`, six to harm and four to heal, doubling a level.
constexpr float kHarm = 6.0F;
constexpr float kHeal = 4.0F;

/// parity: `Arrow.doPostHurtEffects` and `SpectralArrow`'s: the potion the arrow carries, or glowing.
[[nodiscard]] std::vector<world::ActiveEffect> effectsOf(const ServerEntity& arrow) {
    if (arrow.type().path() == "spectral_arrow") {
        return {world::ActiveEffect{.effect = wire::common::Identifier::vanilla("glowing"),
                                    .remainingTicks = kGlowingTicks}};
    }
    const wire::nbt::NbtCompound* contents = arrow.heldItemComponents.getCompound("minecraft:potion_contents");
    if (contents == nullptr) {
        return {};
    }
    const float fallback = arrow.heldItem == std::optional<std::string>{"tipped_arrow"} ? kTippedScale : 1.0F;
    return world::potionEffects::of(*contents,
                                    arrow.heldItemComponents.getFloat("minecraft:potion_duration_scale", fallback));
}

} // namespace

void CombatSystem::arrowEffects(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                const std::vector<ServerEntity*>& living, const ServerEntity& arrow, ServerEntity* mob,
                                PlayerSession* player) {
    for (const world::ActiveEffect& effect : effectsOf(arrow)) {
        if (mob != nullptr) {
            static_cast<void>(giveMobEffect(world, watchers, *mob, effect, arrow.entityId()));
            continue;
        }
        if (player == nullptr) {
            continue;
        }
        const bool harm = effect.effect == world::mobEffects::instantDamage();
        if (harm || effect.effect == world::mobEffects::instantHealth()) {
            const auto amount = static_cast<float>(1 << std::clamp(effect.amplifier, 0, 30));
            if (!harm && player->connection().heal) {
                player->connection().heal(kHeal * amount);
            } else if (harm && player->connection().hurt) {
                player->connection().hurt(kHarm * amount, "indirectMagic",
                                          wire::common::Identifier::vanilla("indirect_magic"),
                                          entity::DamageSource{.entityId = arrow.entityId(),
                                                               .x = arrow.x,
                                                               .z = arrow.z,
                                                               .name = shooterName(watchers, living, arrow),
                                                               .ownerId = arrow.ownerId});
            }
            continue;
        }
        if (player->connection().addEffect) {
            player->connection().addEffect(effect.effect, effect.remainingTicks, effect.amplifier);
        }
    }
}

void CombatSystem::deflectArrow(ServerEntity& arrow) {
    // parity: `ProjectileDeflection.REVERSE` then `scale(0.2)`: back at a tenth of the speed, turned
    // round by a little over half a turn either way.
    constexpr double kReverse = -0.5;
    arrow.velocityX *= kReverse * kArrowBounce;
    arrow.velocityY *= kReverse * kArrowBounce;
    arrow.velocityZ *= kReverse * kArrowBounce;
    const float spin = 170.0F + (static_cast<float>(options_.nextInt ? options_.nextInt(1000) : 0) / 1000.0F * 20.0F);
    arrow.yaw += spin;
    // parity: an arrow left with almost no speed is gone.
    constexpr double kStopped = 1.0E-7;
    if ((arrow.velocityX * arrow.velocityX) + (arrow.velocityY * arrow.velocityY) +
            (arrow.velocityZ * arrow.velocityZ) <
        kStopped) {
        arrow.removed = true;
    }
}

void CombatSystem::arrowTriggers(const world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                 ServerEntity& arrow, const ServerEntity& mob, float damage, bool died) {
    world::AdvancementTriggers* triggers = options_.advancementTriggers;
    const auto shooter = std::ranges::find_if(watchers, [&arrow](const PlayerSession* player) {
        return arrow.ownerId.has_value() && player->entityId() == *arrow.ownerId;
    });
    if (triggers == nullptr || shooter == watchers.end()) {
        return;
    }
    world::triggerFacts::playerHurtEntity(*triggers, world, **shooter, mob, damage, world::damageTypes::kArrow,
                                          world::triggerFacts::ofEntity(world, arrow));
    // parity: `AbstractArrow.onHitEntity`: a piercing arrow names all it killed on every hit, any
    // other only the one it just killed.
    std::vector<wire::common::Identifier> killed;
    if (arrow.pierceLevel > 0) {
        if (died) {
            arrow.piercedAndKilled.push_back(mob.type());
        }
        killed = arrow.piercedAndKilled;
    } else if (died) {
        killed.push_back(mob.type());
    } else {
        return;
    }
    world::TriggerEvent event;
    std::set<wire::common::Identifier> kinds;
    for (const wire::common::Identifier& type : killed) {
        event.victims.push_back(server::world::loot::EntityFacts{.type = type});
        kinds.insert(type);
    }
    event.numbers.emplace("unique_entity_types", static_cast<double>(kinds.size()));
    if (arrow.firedFromWeapon.has_value()) {
        event.items.emplace("fired_from_weapon", world::TriggerItem{.item = *arrow.firedFromWeapon,
                                                                    .components = arrow.firedFromWeaponComponents});
    }
    triggers->fire("killed_by_arrow", (*shooter)->name(), (*shooter)->uuid().toString(), event);
}

CombatSystem::ArrowHit CombatSystem::hitEntityAlong(world::LoadedWorld& world,
                                                    const std::vector<PlayerSession*>& watchers,
                                                    const std::vector<ServerEntity*>& living, ServerEntity& arrow,
                                                    double fromX, double fromY, double fromZ) {
    const std::optional<Target> target = targetAlong(watchers, living, arrow, fromX, fromY, fromZ);
    if (!target.has_value()) {
        return ArrowHit::Missed;
    }
    // parity: `AbstractArrow.onHitEntity`: the damage is the speed times the arrow's own two points,
    // rounded up.
    const double speed = std::sqrt((arrow.velocityX * arrow.velocityX) + (arrow.velocityY * arrow.velocityY) +
                                   (arrow.velocityZ * arrow.velocityZ));
    auto damage = static_cast<std::int32_t>(std::ceil(speed * kArrowBaseDamage));
    // parity: a piercing arrow counts what it went through, and one past its level is spent.
    const bool piercing = arrow.pierceLevel > 0;
    if (piercing) {
        if (arrow.piercedIds.size() >= static_cast<std::size_t>(arrow.pierceLevel) + 1) {
            arrow.removed = true;
            return ArrowHit::Stopped;
        }
        arrow.piercedIds.push_back(target->mob != nullptr ? target->mob->entityId() : target->player->entityId());
    }
    // parity: a fully drawn bow adds a roll on top, after the piercing count.
    if (arrow.critical && options_.nextInt) {
        damage += options_.nextInt((damage / 2) + 2);
    }

    if (target->mob != nullptr && world::endCrystals::isCrystal(target->mob->type())) {
        arrow.removed = true;
        hitCrystal(world, watchers, *target->mob, false, false,
                   firedByPlayer(watchers, arrow) ? arrow.ownerId : std::nullopt);
        return ArrowHit::Stopped;
    }
    if (target->mob != nullptr) {
        ServerEntity& mob = *target->mob;
        if (mob.dragon != nullptr) {
            mob.dragon->hitPart = target->dragonPart;
        }
        markWitherFriend(world, mob, arrow.ownerId);
        entity::entityCombat::blame(mob, arrow.ownerId, arrow.entityId(), arrow.type());
        const entity::entityCombat::Result result =
            entity::entityCombat::hurt(mob, static_cast<float>(damage), fromX, fromZ, hooks_, "projectile",
                                       std::nullopt, firedByPlayer(watchers, arrow));
        if (!result.landed) {
            // parity: an arrow that found somebody still inside their hurt cooldown bounces off.
            deflectArrow(arrow);
            return ArrowHit::Stopped;
        }
        emitVibration(world, "entity_damage", mob.x, mob.y, mob.z, mob.uuid());
        // parity: `AbstractArrow.onHitEntity` leaves a non-piercing arrow stuck in it.
        if (!piercing) {
            ++mob.arrowCount;
        }
        // parity: `Warden.hurtServer` angers it at whoever shot, and fights them within five blocks.
        if (arrow.ownerId.has_value() && entity::wardens::isWarden(mob.type())) {
            static_cast<void>(entity::wardens::anger(mob, *arrow.ownerId, entity::wardens::kHurt));
            for (const PlayerSession* shooter : watchers) {
                if (shooter->entityId() == *arrow.ownerId) {
                    const double dx = shooter->x - mob.x;
                    const double dy = shooter->y - mob.y;
                    const double dz = shooter->z - mob.z;
                    MobActionSystem::wardenStruck(mob, *arrow.ownerId, false, (dx * dx) + (dy * dy) + (dz * dz));
                }
            }
        }
        arrow.removed = !piercing;
        rememberShooter(watchers, arrow, mob);
        if (arrow.ownerId.has_value()) {
            mob.hurtBy = *arrow.ownerId;
        }
        mob.panicTicks = kPanicTicks;
        announceEntityHit(watchers, mob, world::damageTypes::kArrow, arrow.entityId(), result.died);
        arrowHitSound(watchers, arrow);
        arrowTriggers(world, watchers, arrow, mob, static_cast<float>(damage), result.died);
        if (!result.died) {
            arrowEffects(world, watchers, living, arrow, &mob, nullptr);
        }
        return piercing ? ArrowHit::Pierced : ArrowHit::Stopped;
    }
    // parity: a raised shield makes `hurtServer` answer false and the arrow turns back, unless it
    // pierces, which no shield stops.
    const bool shielded = !piercing && player::shieldFaces(*target->player, fromX, fromZ);
    if (target->player->connection().hurt) {
        target->player->connection().hurt(static_cast<float>(damage), kDeathArrow, world::damageTypes::kArrow,
                                          entity::DamageSource{.entityId = arrow.entityId(),
                                                               .x = fromX,
                                                               .z = fromZ,
                                                               .name = shooterName(watchers, living, arrow),
                                                               .ownerId = arrow.ownerId,
                                                               .piercing = piercing});
    }
    if (shielded) {
        deflectArrow(arrow);
        return ArrowHit::Stopped;
    }
    arrowHitSound(watchers, arrow);
    if (!target->player->dead) {
        arrowEffects(world, watchers, living, arrow, nullptr, target->player);
    }
    arrow.removed = !piercing;
    return piercing ? ArrowHit::Pierced : ArrowHit::Stopped;
}

void CombatSystem::arrowHitSound(const std::vector<PlayerSession*>& watchers, const ServerEntity& arrow) {
    if (arrow.sourceNbt.has_value() && arrow.sourceNbt->getBoolean("Silent")) {
        return;
    }
    const bool crossbow = !arrow.arrowLanded && arrow.firedFromWeapon.has_value() &&
                          *arrow.firedFromWeapon == wire::common::Identifier::vanilla("crossbow");
    const std::string sound = crossbow ? "item.crossbow.hit" : "entity.arrow.hit";
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    const float pitch = 1.2F / ((random.nextFloat() * 0.2F) + 0.9F);
    for (PlayerSession* watcher : watchers) {
        if (watcher->connection().playSound) {
            watcher->connection().playSound(sound, arrow.x, arrow.y, arrow.z, 1.0F, pitch);
            continue;
        }
        if (watcher->connection().sendPlayPacket) {
            const wire::protocol::play::SoundPacket packet{.sound = sound,
                                                           .source = wire::protocol::play::SoundSource::Neutral,
                                                           .x = arrow.x,
                                                           .y = arrow.y,
                                                           .z = arrow.z,
                                                           .volume = 1.0F,
                                                           .pitch = pitch,
                                                           .seed = random.nextLong()};
            watcher->connection().sendPlayPacket(wire::protocol::play::SoundPacket::kId,
                                                 [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

} // namespace basalt::server::runtime
