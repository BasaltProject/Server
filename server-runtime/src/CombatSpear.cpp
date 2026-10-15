#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerAttributes.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/PlayerSpear.hpp>
#include <basalt/server/runtime/player/PlayerVersusPlayer.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PostPiercingEffects.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WeaponDamage.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::Gamemode;
using wire::protocol::play::SoundSource;
namespace mth = wire::common::mth;
namespace spear = player::spear;

/// parity: `Player.stabAttack`'s own push, before the weapon's.
constexpr double kStabKnockback = 0.4;
constexpr float kStabExhaustion = 0.1F;
/// parity: `EntityEvent` 2, `onKineticHit` on the client.
constexpr std::int32_t kKineticHitEvent = 2;
constexpr std::int32_t kPanicTicks = 40;
constexpr double kTicksPerSecond = 20.0;
/// parity: `ApplyEntityImpulse`'s `applyPostImpulseGraceTime(10)`.
constexpr std::int32_t kLungeGraceTicks = 10;

[[nodiscard]] double dot(const std::array<double, 3>& one, const std::array<double, 3>& other) {
    return (one[0] * other[0]) + (one[1] * other[1]) + (one[2] * other[2]);
}

[[nodiscard]] spear::Box boxOf(double x, double y, double z, double width, double height) {
    return spear::Box{x - (width / 2.0), y, z - (width / 2.0), x + (width / 2.0), y + height, z + (width / 2.0)};
}

[[nodiscard]] Identifier idOf(const std::string& id) {
    const auto parsed = Identifier::parse(id);
    return parsed.has_value() ? *parsed : Identifier::vanilla("air");
}

} // namespace

std::vector<spear::Candidate> CombatHandler::spearTargets(const PlayerSession& session, world::LoadedWorld& world) {
    std::vector<spear::Candidate> candidates;
    // parity: `PiercingWeapon.canHitEntity`.
    // rides the same vehicle.
    const auto sameVehicle = [&session](const std::optional<std::int32_t>& vehicle) {
        return session.vehicleId.has_value() && vehicle == session.vehicleId;
    };
    for (ServerEntity* candidate : world.state().entities()) {
        if (candidate->removed || candidate->health <= 0.0 || candidate->dragon != nullptr ||
            !server::registry::entityAttributes::isLiving(candidate->type()) || sameVehicle(candidate->vehicleId) ||
            (session.vehicleId == candidate->entityId())) {
            continue;
        }
        candidates.push_back(
            {.id = candidate->entityId(),
             .box = boxOf(candidate->x, candidate->y, candidate->z, candidate->width(), candidate->height())});
    }
    // parity: `ServerPlayer.canHarmPlayer`, nobody while pvp is off.
    if (!world.gameRules().get(world::gameRules::kPvp)) {
        return candidates;
    }
    const Identifier playerType = Identifier::vanilla("player");
    for (const PlayerSession* other : owner_->playersInSameWorld()) {
        if (other->entityId() == session.entityId() || other->dead || other->gamemode == Gamemode::Spectator ||
            other->invulnerable() || sameVehicle(other->vehicleId)) {
            continue;
        }
        candidates.push_back(
            {.id = other->entityId(),
             .box = boxOf(other->x, other->y, other->z, server::registry::entityTypeShapes::widthOf(playerType),
                          server::registry::entityTypeShapes::heightOf(playerType))});
    }
    return candidates;
}

void CombatHandler::soundAroundExceptSelf(const PlayerSession& session, std::string_view sound) {
    const std::int64_t seed = options_.nextSeed ? options_.nextSeed() : 0;
    for (const PlayerSession* other : owner_->playersInSameWorld()) {
        if (other->entityId() != session.entityId()) {
            player::playSoundFor(*other, sound, SoundSource::Players, session.x, session.y, session.z, 1.0F, 1.0F,
                                 seed);
        }
    }
}

void CombatHandler::jab() {
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || session->dead || session->gamemode == Gamemode::Spectator || world == nullptr) {
        return;
    }
    const std::int32_t slot = PlayerInventory::kHotbarStart + session->inventory.heldSlot();
    const std::optional<storage::StoredItemStack> held = session->inventory.at(slot);
    const spear::Spear* weapon = held.has_value() ? spear::of(held->id) : nullptr;
    world::RealEnchantments* enchantments = owner_->options().enchantments;
    const player::playerAttributes::DefinitionOf definitionOf = [enchantments](const Identifier& id) {
        return enchantments == nullptr ? nullptr : enchantments->definitionOf(id);
    };
    const double attackSpeed = player::playerAttributes::valueOf(*session, "minecraft:attack_speed", definitionOf);
    // parity: `cannotAttackWithItem(stack, 5)`, and only a `piercing_weapon` jabs at all.
    if (weapon == nullptr || spear::tooSoon(session->ticksSinceSwing, attackSpeed, spear::kJabTolerance)) {
        return;
    }
    const auto damage =
        static_cast<float>(player::playerAttributes::valueOf(*session, "minecraft:attack_damage", definitionOf));
    const float strength = player::combat::attackStrengthScale(session->ticksSinceSwing, attackSpeed);
    const std::array<double, 3> look = spear::lookOf(session->yaw, session->pitch);
    const std::array<double, 3> eye{session->x, session->y + world::fluids::eyeHeight(session->sneaking), session->z};
    bool hit = false;
    for (const std::int32_t target :
         spear::hitsAlong(world->state(), eye, look, dot(session->knownMovement, look),
                          session->gamemode == Gamemode::Creative, spearTargets(*session, *world))) {
        // parity: the jab deals knockback and never dismounts.
        hit |= stab(*session, *world, target, slot, damage, true, true, false, strength);
    }
    postPiercingAttack(*session, *world, slot);
    if (hit) {
        owner_->broadcastSound(weapon->hitSound, session->x, session->y, session->z, SoundSource::Players);
    }
    soundAroundExceptSelf(*session, weapon->attackSound);
    // parity: `onAttack` and `swingAndResetAttackStrength`, the stab everyone else sees.
    owner_->blockInteraction().handlePunch();
}

bool CombatHandler::startCharge(PlayerSession& session, std::int32_t slot) {
    const std::optional<storage::StoredItemStack> held = session.inventory.at(slot);
    const spear::Spear* weapon = held.has_value() ? spear::of(held->id) : nullptr;
    if (weapon == nullptr) {
        return false;
    }
    chargeSlot_ = slot;
    chargeTicks_ = 0;
    stabbed_.clear();
    owner_->itemUseTimers().setUsingItem(true, idOf(held->id), slot == PlayerInventory::kOffhandSlot);
    soundAroundExceptSelf(session, weapon->useSound);
    return true;
}

void CombatHandler::stopCharge() {
    if (!chargeSlot_.has_value()) {
        return;
    }
    chargeSlot_.reset();
    stabbed_.clear();
    owner_->itemUseTimers().setUsingItem(false);
}

void CombatHandler::tickCharge(PlayerSession& session) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (!chargeSlot_.has_value() || world == nullptr) {
        return;
    }
    const std::int32_t slot = *chargeSlot_;
    const std::optional<storage::StoredItemStack> held = session.inventory.at(slot);
    const bool inHand =
        slot == PlayerInventory::kOffhandSlot || slot == PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    const spear::Spear* weapon = held.has_value() && inHand ? spear::of(held->id) : nullptr;
    if (weapon == nullptr || session.dead) {
        stopCharge();
        return;
    }
    // parity: `KineticWeapon.damageEntities`.
    const std::int32_t used = chargeTicks_++ - weapon->delayTicks;
    if (used < 0) {
        return;
    }
    const std::array<double, 3> look = spear::lookOf(session.yaw, session.pitch);
    // parity: `getMotion`, what the client moved it by, or a vehicle it does not drive.
    std::array<double, 3> motion = session.knownMovement;
    if (session.vehicleId.has_value()) {
        if (const ServerEntity* vehicle = world->state().entity(*session.vehicleId);
            vehicle != nullptr && !controls(session, *vehicle)) {
            motion = {vehicle->velocityX, vehicle->velocityY, vehicle->velocityZ};
        }
    }
    const double attackerSpeed = dot(look, motion) * kTicksPerSecond;
    const std::array<double, 3> eye{session.x, session.y + world::fluids::eyeHeight(session.sneaking), session.z};
    const std::int64_t now = world->gameTime();
    const auto base = static_cast<float>(player::playerAttributes::baseOf(session, "minecraft:attack_damage"));
    bool affected = false;
    for (const std::int32_t target :
         spear::hitsAlong(world->state(), eye, look, dot(session.knownMovement, look),
                          session.gamemode == Gamemode::Creative, spearTargets(session, *world))) {
        const auto seen = std::ranges::find(stabbed_, target, &std::pair<std::int32_t, std::int64_t>::first);
        if (seen != stabbed_.end() && now - seen->second < weapon->contactCooldownTicks) {
            continue;
        }
        if (seen != stabbed_.end()) {
            seen->second = now;
        } else {
            stabbed_.emplace_back(target, now);
        }
        std::array<double, 3> targetMotion{};
        if (const ServerEntity* mob = world->state().entity(target); mob != nullptr) {
            targetMotion = {mob->velocityX, mob->velocityY, mob->velocityZ};
        } else {
            for (const PlayerSession* other : owner_->playersInSameWorld()) {
                if (other->entityId() == target) {
                    targetMotion = other->knownMovement;
                }
            }
        }
        const double relative = std::max(0.0, attackerSpeed - (dot(look, targetMotion) * kTicksPerSecond));
        const bool dismounts =
            weapon->dismount.test(used, attackerSpeed, relative, static_cast<double>(spear::kPlayerFactor));
        const bool knocks =
            weapon->knockback.test(used, attackerSpeed, relative, static_cast<double>(spear::kPlayerFactor));
        const bool damages =
            weapon->damage.test(used, attackerSpeed, relative, static_cast<double>(spear::kPlayerFactor));
        if (dismounts || knocks || damages) {
            const float dealt =
                base + static_cast<float>(std::floor(relative * static_cast<double>(weapon->damageMultiplier)));
            affected |= stab(session, *world, target, slot, dealt, damages, knocks, dismounts, std::nullopt);
        }
    }
    if (!affected) {
        return;
    }
    broadcastEntityEvent(session.entityId(), kKineticHitEvent);
    if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
        world::TriggerEvent event;
        event.numbers.emplace("count", static_cast<double>(stabbed_.size()));
        triggers->fire("spear_mobs", session.name(), session.uuid().toString(), event);
    }
}

bool CombatHandler::stab(PlayerSession& session, world::LoadedWorld& world, std::int32_t targetId, std::int32_t slot,
                         float base, bool dealsDamage, bool dealsKnockback, bool dismounts,
                         std::optional<float> strength) {
    ServerEntity* mob = world.state().entity(targetId);
    PlayerSession* victim = nullptr;
    if (mob == nullptr) {
        for (PlayerSession* other : owner_->playersInSameWorld()) {
            if (other->entityId() == targetId) {
                victim = other;
            }
        }
        if (victim == nullptr) {
            return false;
        }
    }
    if (hooks_.attackAllowed && !hooks_.attackAllowed(session, targetId)) {
        return false;
    }
    const std::optional<storage::StoredItemStack> weapon = session.inventory.at(slot);
    world::RealEnchantments* enchantments = owner_->options().enchantments;
    const world::weaponDamage::DefinitionOf definitionOf = [enchantments](const Identifier& id) {
        return enchantments == nullptr ? nullptr : enchantments->definitionOf(id);
    };
    const Identifier victimType = mob != nullptr ? mob->type() : Identifier::vanilla("player");
    const world::realEntityTypeTags::TagMembers* entityTags = owner_->options().entityTypeTags;
    const world::weaponDamage::Victim target{
        .type = victimType, .inTag = [entityTags, &victimType](const Identifier& tag) {
            return entityTags != nullptr && world::realEntityTypeTags::membersOf(*entityTags, tag).contains(victimType);
        }};
    // parity: `Player.stabAttack`.
    // while a charge in that hand is not.
    float magic = world::weaponDamage::of(weapon, base, definitionOf, &target) - base;
    if (strength.has_value()) {
        magic *= *strength;
        base *= player::combat::chargeScale(*strength);
    }
    const float total = dealsDamage ? base + magic : 0.0F;
    const float yaw = session.yaw * (std::numbers::pi_v<float> / 180.0F);
    const auto towardsX = static_cast<double>(mth::sin(yaw));
    const auto towardsZ = static_cast<double>(-mth::cos(yaw));
    // parity: `getKnockback`, the attribute plus the knockback enchantment, halved.
    const float extra =
        (static_cast<float>(player::playerAttributes::valueOf(session, "minecraft:attack_knockback", definitionOf)) +
         (weapon.has_value() ? world::weaponDamage::valueEffect(*weapon, "minecraft:knockback", 0.0F, definitionOf)
                             : 0.0F)) /
        2.0F;
    if (victim != nullptr) {
        return stabPlayer(session, *victim, slot,
                          StabBlow{.total = total,
                                   .magic = magic,
                                   .extraKnockback = extra,
                                   .dealsDamage = dealsDamage,
                                   .dealsKnockback = dealsKnockback,
                                   .dismounts = dismounts});
    }
    bool hurt = false;
    bool died = false;
    const double before = mob->health;
    if (dealsDamage) {
        entity::entityCombat::blame(*mob, session.entityId(), session.entityId(), Identifier::vanilla("player"));
        // parity: `spear` is in `no_knockback`, so the hit itself pushes nothing.
        const std::array<double, 3> kept{mob->velocityX, mob->velocityY, mob->velocityZ};
        const entity::entityCombat::Result result = entity::entityCombat::hurt(
            *mob, total, session.x, session.z, {}, "spear", std::string{session.name()}, true);
        mob->velocityX = kept[0];
        mob->velocityY = kept[1];
        mob->velocityZ = kept[2];
        hurt = result.landed;
        died = result.died;
    }
    if (dealsKnockback) {
        entity::entityCombat::knockback(*mob, kStabKnockback, towardsX, towardsZ);
        if (extra > 0.0F) {
            entity::entityCombat::knockback(*mob, static_cast<double>(extra), towardsX, towardsZ);
        }
    }
    bool dismounted = false;
    const world::realEntityTypeTags::TagMembers* tags = owner_->options().entityTypeTags;
    if (dismounts && mob->vehicleId.has_value() &&
        (tags == nullptr ||
         !world::realEntityTypeTags::membersOf(*tags, Identifier::vanilla("cannot_be_dismounted_by_item_usage"))
              .contains(mob->type()))) {
        if (ServerEntity* vehicle = world.state().entity(*mob->vehicleId); vehicle != nullptr) {
            std::erase(vehicle->passengerIds, mob->entityId());
            broadcastPassengers(*vehicle);
        }
        mob->vehicleId.reset();
        dismounted = true;
    }
    if (!hurt && !dealsKnockback && !dismounted) {
        return false;
    }
    if (magic > 0.0F) {
        broadcastAnimation(mob->entityId(), wire::protocol::play::AnimatePacket::kMagicCriticalHit);
    }
    if (hurt) {
        if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
            world::triggerFacts::playerHurtEntity(*triggers, world, session, *mob, total, world::damageTypes::kSpear);
        }
        world::vibrations::emit(world.state(), "entity_damage", mob->x, mob->y, mob->z, world.gameTime(),
                                {.entity = mob->uuid()});
        broadcastDamageEvent(*mob, world::damageTypes::kSpear, session.entityId());
        mob->hurtBy = session.entityId();
        entity::entityCombat::rememberPlayer(*mob, session.uuid());
        if (entity::wardens::isWarden(mob->type())) {
            entity::wardens::anger(*mob, session.entityId(), entity::wardens::kHurt);
            MobActionSystem::wardenStruck(*mob, session.entityId(), true, 0.0);
        }
        mob->panicTicks = kPanicTicks;
    }
    broadcastMotion(*mob);
    // parity: `itemAttackInteraction`.
    if (weapon.has_value() && item::itemWear::costPerAttack(*weapon) > 0) {
        player::statAwards::used(session, weapon->id);
        player::wearItemIn(owner_->blockInteraction().breakContext(), session, slot,
                           item::itemWear::costPerAttack(*weapon));
    }
    // parity: `damageStatsAndHearts`, the stat and the dark hearts for what the health lost.
    const auto lost = static_cast<float>(before - std::max(mob->health, 0.0));
    session.stats.addDamageDealt(lost);
    player::pvp::showDamageIndicator(owner_->playersInSameWorld(), mob->x, mob->y + (mob->height() * 0.5), mob->z,
                                     lost);
    session.food.addExhaustion(kStabExhaustion);
    if (died) {
        killEntity(*mob);
    } else if (hurt) {
        owner_->broadcastSound(entity::entityCombat::hurtSound(*mob), mob->x, mob->y, mob->z, SoundSource::Hostile);
    }
    return true;
}

bool CombatHandler::stabPlayer(PlayerSession& session, PlayerSession& victim, std::int32_t slot, const StabBlow& blow) {
    const std::optional<storage::StoredItemStack> weapon = session.inventory.at(slot);
    const float before = victim.health();
    const std::array<double, 3> oldMovement = victim.deltaMovement;
    victim.lastHurtLanded = false;
    if (blow.dealsDamage && victim.connection().hurt) {
        victim.connection().hurt(blow.total, "spear", world::damageTypes::kSpear,
                                 entity::DamageSource{.entityId = session.entityId(),
                                                      .x = session.x,
                                                      .z = session.z,
                                                      .name = wire::text::Component::text(session.name()),
                                                      .ownerId = session.entityId()});
    }
    const bool hurt = blow.dealsDamage && victim.lastHurtLanded;
    if (blow.dealsKnockback && !victim.dead) {
        world::RealEnchantments* enchantments = owner_->options().enchantments;
        const world::weaponDamage::DefinitionOf definitionOf = [enchantments](const Identifier& id) {
            return enchantments == nullptr ? nullptr : enchantments->definitionOf(id);
        };
        const double resistance =
            player::playerAttributes::valueOf(victim, "minecraft:knockback_resistance", definitionOf);
        const float yaw = session.yaw * (std::numbers::pi_v<float> / 180.0F);
        const auto towardsX = static_cast<double>(mth::sin(yaw));
        const auto towardsZ = static_cast<double>(-mth::cos(yaw));
        const auto fromYaw =
            static_cast<float>(std::atan2(session.z - victim.z, session.x - victim.x) * 180.0 / std::numbers::pi) -
            victim.yaw;
        // parity: each push starts again from the motion before the blow, and goes out on its own.
        for (const double power : {kStabKnockback, static_cast<double>(blow.extraKnockback)}) {
            if (power <= 0.0) {
                continue;
            }
            player::pvp::sendMotion(victim,
                                    player::combat::knockback(power, towardsX, towardsZ, oldMovement[0], oldMovement[1],
                                                              oldMovement[2], victim.onGround, resistance),
                                    fromYaw);
        }
        victim.deltaMovement = oldMovement;
        session.sprinting = false;
    }
    // parity: `stopRiding`; a player is never in `cannot_be_dismounted_by_item_usage`.
    bool dismounted = false;
    if (blow.dismounts && victim.vehicleId.has_value() && victim.connection().dismount) {
        victim.connection().dismount();
        dismounted = true;
    }
    if (!hurt && !blow.dealsKnockback && !dismounted) {
        return false;
    }
    if (blow.magic > 0.0F) {
        broadcastAnimation(victim.entityId(), wire::protocol::play::AnimatePacket::kMagicCriticalHit);
    }
    // parity: `itemAttackInteraction`.
    if (weapon.has_value() && item::itemWear::costPerAttack(*weapon) > 0) {
        player::statAwards::used(session, weapon->id);
        player::wearItemIn(owner_->blockInteraction().breakContext(), session, slot,
                           item::itemWear::costPerAttack(*weapon));
    }
    const float lost = before - std::max(victim.health(), 0.0F);
    session.stats.addDamageDealt(lost);
    player::pvp::showDamageIndicator(
        owner_->playersInSameWorld(), victim.x,
        victim.y + (server::registry::entityTypeShapes::heightOf(Identifier::vanilla("player")) * 0.5), victim.z, lost);
    session.food.addExhaustion(kStabExhaustion);
    return true;
}

void CombatHandler::postPiercingAttack(PlayerSession& session, world::LoadedWorld& world, std::int32_t slot) {
    const std::optional<storage::StoredItemStack> weapon = session.inventory.at(slot);
    world::RealEnchantments* enchantments = owner_->options().enchantments;
    if (!weapon.has_value() || enchantments == nullptr) {
        return;
    }
    const std::optional<std::int32_t> feet =
        world.state().blockState(mth::floor(session.x), mth::floor(session.y), mth::floor(session.z));
    const world::postPiercing::User user{
        .player = true,
        .riding = session.vehicleId.has_value(),
        .fallFlying = session.fallFlying,
        .inWater = feet.has_value() && world::fluids::of(*feet) == world::Submersion::Water,
        .onGround = session.onGround,
        .sneaking = session.sneaking,
        .sprinting = session.sprinting,
        .gamemode = session.gamemode == Gamemode::Creative    ? "creative"
                    : session.gamemode == Gamemode::Adventure ? "adventure"
                    : session.gamemode == Gamemode::Spectator ? "spectator"
                                                              : "survival",
        .food = session.food.foodLevel(),
        .yaw = session.yaw,
        .pitch = session.pitch,
    };
    const world::postPiercing::Outcome outcome = world::postPiercing::of(
        *weapon, [enchantments](const Identifier& id) { return enchantments->definitionOf(id); }, user);
    if (outcome.itemDamage > 0) {
        player::wearItemIn(owner_->blockInteraction().breakContext(), session, slot, outcome.itemDamage);
    }
    if (outcome.exhaustion > 0.0F) {
        session.food.addExhaustion(outcome.exhaustion);
    }
    if (outcome.pushed) {
        // parity: `ApplyEntityImpulse` adds to the motion and sends a player the sum at once.
        auto& [dx, dy, dz] = session.deltaMovement;
        dx += outcome.impulse[0];
        dy += outcome.impulse[1];
        dz += outcome.impulse[2];
        if (session.connection().pushBack) {
            session.connection().pushBack(dx, dy, dz, session.yaw);
        } else if (session.connection().sendPlayPacket) {
            const wire::protocol::play::SetEntityMotionPacket packet{
                .entityId = session.entityId(), .velocityX = dx, .velocityY = dy, .velocityZ = dz};
            session.connection().sendPlayPacket(wire::protocol::play::SetEntityMotionPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
        session.impulseGraceTicks = std::max(session.impulseGraceTicks, kLungeGraceTicks);
    }
    for (const world::postPiercing::Sound& sound : outcome.sounds) {
        owner_->broadcastSound(sound.name, session.x, session.y, session.z, SoundSource::Players, sound.volume,
                               sound.pitch);
    }
}

} // namespace basalt::server::runtime
