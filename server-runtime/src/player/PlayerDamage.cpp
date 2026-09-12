#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/DamageProtection.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace basalt::server::runtime::player {
namespace {

using wire::common::Identifier;

/// parity: `LivingEntity.invulnerableDuration`, and the half of it inside which only a bigger hit
/// lands.
constexpr std::int32_t kHurtCooldownTicks = 20;
constexpr std::int32_t kCooldownHalfTicks = 10;

constexpr double kRadiansToDegrees = 180.0 / std::numbers::pi;

/// Tells everyone in the player's world that they were hit, through each one's own connection.
void showDamage(const PlayerSession& session, const std::optional<Identifier>& damageType,
                const std::int32_t causeEntityId, const DamageContext& context) {
    // Held by value: a `path()` off the optional is a view into a temporary once the ternary ends.
    const std::string path = damageType.has_value() ? std::string{damageType->path()} : std::string{"generic"};
    const std::vector<registry::vanillaRegistryData::Entry>& types = registry::vanillaRegistryData::damageTypes();
    std::int32_t registryId = -1;
    for (std::size_t index = 0; index < types.size(); index++) {
        if (types[index].first == path) {
            registryId = static_cast<std::int32_t>(index);
            break;
        }
    }
    if (registryId < 0) {
        return;
    }
    if (context.registry == nullptr) {
        return;
    }
    const wire::protocol::play::DamageEventPacket packet{.entityId = session.entityId(),
                                                         .damageType = registryId,
                                                         .causeEntityId = causeEntityId,
                                                         .directEntityId = causeEntityId};
    for (const PlayerSession* player : context.registry->snapshot()) {
        if (player->worldName != session.worldName) {
            continue;
        }
        if (player->connection().showDamage) {
            player->connection().showDamage(session.entityId(), registryId, causeEntityId);
            continue;
        }
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::DamageEventPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

/// parity: `LivingEntity.knockback` as it lands on a player, plus the hurt animation.
void pushAway(const PlayerSession& session, const entity::DamageSource& source) {
    const double towardsX = source.x - session.x;
    const double towardsZ = source.z - session.z;
    const std::array<double, 3> moved = player::combat::knockback(entity::entityCombat::kDefaultKnockback, towardsX,
                                                                  towardsZ, 0.0, 0.0, 0.0, session.onGround);
    const auto angle = static_cast<float>(std::atan2(towardsZ, towardsX) * kRadiansToDegrees) - session.yaw;
    if (session.connection().pushBack) {
        session.connection().pushBack(moved[0], moved[1], moved[2], angle);
        return;
    }
    if (!session.connection().sendPlayPacket) {
        return;
    }
    session.connection().sendPlayPacket(
        wire::protocol::play::SetEntityMotionPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::SetEntityMotionPacket{
                .entityId = session.entityId(), .velocityX = moved[0], .velocityY = moved[1], .velocityZ = moved[2]}
                .write(buffer);
        });
    session.connection().sendPlayPacket(
        wire::protocol::play::HurtAnimationPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::HurtAnimationPacket{.entityId = session.entityId(), .yaw = angle}.write(buffer);
        });
}

} // namespace

float damageAfterProtection(const PlayerSession& session, const float amount,
                            const std::optional<Identifier>& damageType, const DamageContext& context) {
    const world::realDamageTypeTags::TagMembers* tags = context.damageTypeTags;
    if (tags == nullptr || !damageType.has_value()) {
        return amount;
    }
    float damage = amount;
    std::vector<std::optional<storage::StoredItemStack>> worn;
    for (std::int32_t slot = PlayerInventory::kArmourStart;
         slot < PlayerInventory::kArmourStart + PlayerInventory::kArmourSlots; slot++) {
        worn.push_back(session.inventory.at(slot));
    }
    if (!world::realDamageTypeTags::isIn(*tags, world::realDamageTypeTags::kBypassesArmour, *damageType)) {
        float armour = 0.0F;
        float toughness = 0.0F;
        for (const std::optional<storage::StoredItemStack>& stack : worn) {
            if (!stack.has_value()) {
                continue;
            }
            const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(stack->id);
            const server::registry::ArmourPiece* piece =
                id.has_value() ? server::registry::itemArmour::of(*id) : nullptr;
            if (piece == nullptr) {
                continue;
            }
            armour += piece->armour;
            toughness += piece->toughness;
        }
        if (armour > 0.0F) {
            damage = player::combat::afterArmour(damage, armour, toughness);
        }
    }
    if (world::realDamageTypeTags::isIn(*tags, world::realDamageTypeTags::kBypassesEffects, *damageType)) {
        return damage;
    }
    const auto resistance = session.effects.find(world::mobEffects::resistance());
    if (resistance != session.effects.end() &&
        !world::realDamageTypeTags::isIn(*tags, world::realDamageTypeTags::kBypassesResistance, *damageType)) {
        damage = player::combat::afterResistance(damage, resistance->second.amplifier);
    }
    if (damage <= 0.0F ||
        world::realDamageTypeTags::isIn(*tags, world::realDamageTypeTags::kBypassesEnchantments, *damageType)) {
        return damage;
    }
    world::RealEnchantments* enchantments = context.enchantments;
    if (enchantments == nullptr) {
        return damage;
    }
    std::vector<world::damageProtection::EnchantmentLevels> levels;
    levels.reserve(worn.size());
    for (const std::optional<storage::StoredItemStack>& stack : worn) {
        levels.push_back(stack.has_value() ? player::enchantmentLevels(*stack)
                                           : world::damageProtection::EnchantmentLevels{});
    }
    const float protection = world::damageProtection::of(
        levels, &*damageType,
        {.definitionOf = [enchantments](const Identifier& id) { return enchantments->definitionOf(id); },
         .isDamageTypeIn =
             [tags](const Identifier& tag, const Identifier& type) {
                 return world::realDamageTypeTags::isIn(*tags, tag, type);
             }});
    return protection > 0.0F ? player::combat::afterProtection(damage, protection) : damage;
}

void hurtPlayer(PlayerSession& player, const float amount, const std::string_view deathMessageId,
                const std::optional<Identifier>& damageType, const std::optional<entity::DamageSource>& source,
                const DamageContext& context) {
    PlayerSession* session = &player;
    if (session->dead || session->invulnerable()) {
        return;
    }
    // A player is an entity too, so it is the same hook; what changes is that the choke point for a
    // player is here rather than in the entity's own combat.
    float incoming = amount;
    if (context.damaged) {
        const std::optional<float> allowed =
            context.damaged(*session, amount, damageType.has_value() ? damageType->path() : deathMessageId,
                            source.has_value() ? std::optional{source->entityId} : std::nullopt);
        if (!allowed.has_value() || *allowed <= 0.0F) {
            return;
        }
        incoming = *allowed;
    }
    // parity: `LivingEntity.hurt`'s cooldown: inside the half second after a hit, only a bigger one
    // lands, and only by the difference. It is what stops lava and burning stacking.
    const float dealt = incoming;
    const bool cooling = session->invulnerableTicks > kCooldownHalfTicks;
    if (cooling) {
        if (dealt <= session->lastHurt) {
            return;
        }
        incoming = dealt - session->lastHurt;
        session->lastHurt = dealt;
    } else {
        session->lastHurt = dealt;
        session->invulnerableTicks = kHurtCooldownTicks;
    }
    const float afterArmour = damageAfterProtection(*session, incoming, damageType, context);
    if (afterArmour <= 0.0F) {
        return;
    }
    if (!cooling) {
        showDamage(*session, damageType, source.has_value() ? source->entityId : -1, context);
        if (source.has_value()) {
            pushAway(*session, *source);
        }
    }
    // parity: `LivingEntity.hurt` takes it out of absorption first, and only the rest off health.
    const float absorbed = std::min(session->absorption, afterArmour);
    if (absorbed > 0.0F) {
        session->absorption -= absorbed;
        if (session->connection().syncAbsorption) {
            session->connection().syncAbsorption();
        }
    }
    const float damage = afterArmour - absorbed;
    if (damage <= 0.0F) {
        return;
    }
    session->stats.addDamageTaken(damage);
    // parity: `LivingEntity.hurtServer` raises `ENTITY_DAMAGE` right where the health comes off, so
    // absorption alone makes none. A player is an entity too and this is their half of that line.
    if (world::LoadedWorld* world = context.world ? context.world() : nullptr; world != nullptr) {
        world::vibrations::emit(world->state(), "entity_damage", session->x, session->y, session->z, world->gameTime());
    }
    session->setHealth(std::max(session->health() - damage, 0.0F));
    if (session->health() <= 0.0F) {
        killPlayer(*session, deathMessageId, source.has_value() ? source->name : std::nullopt, context.death);
    } else if (session->connection().syncHealth) {
        session->connection().syncHealth();
    }
}

void healPlayer(PlayerSession& player, const float amount, const DamageContext& context) {
    PlayerSession* session = &player;
    if (session->dead) {
        return;
    }
    float healed = amount;
    if (context.healed) {
        const std::optional<float> allowed = context.healed(*session, amount);
        if (!allowed.has_value()) {
            return;
        }
        healed = *allowed;
    }
    const float after = std::min(session->health() + healed, session->maxHealth());
    if (after == session->health()) {
        return;
    }
    session->setHealth(after);
    if (session->connection().syncHealth) {
        session->connection().syncHealth();
    }
}

} // namespace basalt::server::runtime::player
