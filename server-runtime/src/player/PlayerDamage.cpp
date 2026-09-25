#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ShieldBlows.hpp>
#include <basalt/server/runtime/item/ItemDamageEffects.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/DamageProtection.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>
#include <vector>

namespace basalt::server::runtime::player {
namespace {

/// parity: the `3.4028235E37F` every damage statistic stays under.
constexpr float kLargestCountedDamage = 3.4028235E37F;

/// parity: `setLastHurtByPlayer(player, 100)`.
constexpr std::int32_t kPlayerHitMemoryTicks = 100;

using wire::common::Identifier;

/// parity: `LivingEntity.invulnerableDuration`, and the half of it inside which only a bigger hit
/// lands.
constexpr std::int32_t kHurtCooldownTicks = 20;
constexpr std::int32_t kCooldownHalfTicks = 10;

constexpr double kRadiansToDegrees = 180.0 / std::numbers::pi;

/// parity: the shield's `blocks_attacks`: a quarter second up, ninety degrees either side, wear from
/// three damage, and an axe's five seconds.
constexpr std::int32_t kShieldDelayTicks = 5;
constexpr float kBlockingAngleDegrees = 90.0F;
constexpr float kShieldWearThreshold = 3.0F;
constexpr float kAxeDisableSeconds = 5.0F;
constexpr double kBlockedPush = 0.5;

/// parity: `Ravager.handleEntityEvent` 39, the stun.
constexpr std::int8_t kStunEvent = 39;

[[nodiscard]] bool tagged(const DamageContext& context, std::string_view tag,
                          const std::optional<Identifier>& damageType) {
    return damageType.has_value() && context.damageTypeTags != nullptr &&
           world::realDamageTypeTags::isIn(*context.damageTypeTags, Identifier::vanilla(std::string{tag}), *damageType);
}

void playAround(const PlayerSession& session, std::string_view sound, float volume, const DamageContext& context) {
    if (context.registry == nullptr) {
        return;
    }
    random::RandomSource random = random::RandomSource::legacy(static_cast<std::int64_t>(std::random_device{}()));
    const float pitch = 0.8F + (random.nextFloat() * 0.4F);
    for (const PlayerSession* heard : context.registry->snapshot()) {
        if (heard->worldName == session.worldName) {
            playSoundFor(*heard, sound, wire::protocol::play::SoundSource::Players, session.x, session.y, session.z,
                         volume, pitch, random.nextLong());
        }
    }
}

/// parity: `LivingEntity.getSecondsToDisableBlocking`, an axe's `weapon` component in the attacker's hand.
[[nodiscard]] float secondsToDisable(std::int32_t attackerId, const DamageContext& context) {
    if (context.registry != nullptr) {
        for (const PlayerSession* attacker : context.registry->snapshot()) {
            if (attacker->entityId() != attackerId) {
                continue;
            }
            const std::optional<storage::StoredItemStack> held =
                attacker->inventory.at(PlayerInventory::kHotbarStart + attacker->inventory.heldSlot());
            return held.has_value() && held->id.ends_with("_axe") ? kAxeDisableSeconds : 0.0F;
        }
    }
    const world::LoadedWorld* world = context.world ? context.world() : nullptr;
    const entity::ServerEntity* mob = world == nullptr ? nullptr : world->state().entity(attackerId);
    // parity: `Warden.getSecondsToDisableBlocking`, five seconds whatever it holds.
    if (mob != nullptr && mob->type().path() == "warden") {
        return kAxeDisableSeconds;
    }
    const wire::nbt::NbtCompound* equipment =
        mob == nullptr || !mob->sourceNbt.has_value() ? nullptr : mob->sourceNbt->getCompound("equipment");
    const wire::nbt::NbtCompound* hand = equipment == nullptr ? nullptr : equipment->getCompound("mainhand");
    return hand != nullptr && hand->getString("id").ends_with("_axe") ? kAxeDisableSeconds : 0.0F;
}

[[nodiscard]] bool facesSource(const PlayerSession& session, double fromX, double fromZ) {
    constexpr auto kRadians = static_cast<float>(std::numbers::pi / 180.0);
    const float yaw = -session.yaw * kRadians;
    const double viewX = static_cast<double>(wire::common::mth::sin(yaw));
    const double viewZ = static_cast<double>(wire::common::mth::cos(yaw));
    double toX = fromX - session.x;
    double toZ = fromZ - session.z;
    const double length = std::sqrt((toX * toX) + (toZ * toZ));
    toX = length < 1.0E-5 ? 0.0 : toX / length;
    toZ = length < 1.0E-5 ? 0.0 : toZ / length;
    const double angle = std::acos((toX * viewX) + (toZ * viewZ));
    return angle <= static_cast<double>(kRadians * kBlockingAngleDegrees);
}

struct Blocked {
    float amount = 0.0F;
    /// Whether the attacker was a body that the shield pushed back against.
    bool pushed = false;
    /// What the attacker's own `blockedByItem` shoved the defender with.
    std::optional<entity::shieldBlows::Push> thrown;
};

/// parity: `LivingEntity.blockUsingItem` hands the blow to the attacker's `blockedByItem`.
[[nodiscard]] std::optional<entity::shieldBlows::Push>
attackerAnswers(const PlayerSession& session, std::int32_t attackerId, const DamageContext& context) {
    world::LoadedWorld* world = context.world ? context.world() : nullptr;
    entity::ServerEntity* attacker = world == nullptr ? nullptr : world->state().entity(attackerId);
    if (attacker == nullptr) {
        return std::nullopt;
    }
    random::RandomSource random = random::RandomSource::legacy(static_cast<std::int64_t>(std::random_device{}()));
    const entity::shieldBlows::Reply reply = entity::shieldBlows::blocked(*attacker, session.x, session.z, 0.0, random);
    // parity: `Ravager.blockedByItem`'s stun: its sound, and event 39 that starts the client's count.
    if (reply.stunned && context.registry != nullptr) {
        const wire::protocol::play::EntityEventPacket stun{.entityId = attacker->entityId(), .eventId = kStunEvent};
        for (const PlayerSession* heard : context.registry->snapshot()) {
            if (heard->worldName != session.worldName) {
                continue;
            }
            playSoundFor(*heard, "entity.ravager.stunned", wire::protocol::play::SoundSource::Hostile, attacker->x,
                         attacker->y, attacker->z, 1.0F, 1.0F, random.nextLong());
            if (heard->connection().sendPlayPacket) {
                heard->connection().sendPlayPacket(wire::protocol::play::EntityEventPacket::kId,
                                                   [&stun](wire::io::ByteBuffer& buffer) { stun.write(buffer); });
            }
        }
    }
    return reply.push;
}

/// parity: `LivingEntity.applyItemBlocking` for a shield, with `hurtBlockingItem` and
/// `Player.blockUsingItem`.
[[nodiscard]] Blocked blockWithShield(PlayerSession& session, float damage, const std::optional<Identifier>& damageType,
                                      const std::optional<entity::DamageSource>& source, const DamageContext& context) {
    if (damage <= 0.0F || !session.blockingSlot.has_value() || session.blockingTicks < kShieldDelayTicks) {
        return {};
    }
    const std::int32_t slot = *session.blockingSlot;
    const std::optional<storage::StoredItemStack> shield = session.inventory.at(slot);
    if (!shield.has_value() || shield->id != "minecraft:shield" || tagged(context, "bypasses_shield", damageType)) {
        return {};
    }
    // parity: a source with no position is taken as straight behind, which nothing blocks.
    if (!source.has_value() || source->piercing || !facesSource(session, source->x, source->z)) {
        return {};
    }
    const float blocked = damage;
    // parity: `BlocksAttacks.hurtBlockingItem`, every blocked hit before any wear.
    statAwards::used(session, shield->id);
    // parity: `ItemDamageFunction`, nothing under three and the floor of one plus the hit above it.
    if (blocked >= kShieldWearThreshold && session.gamemode != wire::protocol::play::Gamemode::Creative) {
        random::RandomSource random = random::RandomSource::legacy(static_cast<std::int64_t>(std::random_device{}()));
        const item::ItemDamageEffects effects{[&context](const Identifier& id) {
            return context.enchantments == nullptr ? nullptr : context.enchantments->definitionOf(id);
        }};
        const std::optional<storage::StoredItemStack> worn = item::itemWear::hurt(
            *shield, static_cast<std::int32_t>(std::floor(1.0F + blocked)), random,
            [&effects](const storage::StoredItemStack& stack, std::int32_t cost, random::RandomSource& source) {
                return effects.process(stack, cost, source);
            });
        session.inventory.set(slot, worn);
        if (context.slotChanged) {
            context.slotChanged(session, slot);
        }
        if (!worn.has_value()) {
            playAround(session, "item.shield.break", 0.8F, context);
            statAwards::itemBroken(session, shield->id,
                                   slot == PlayerInventory::kOffhandSlot ? statAwards::kBreakOffHand
                                                                         : statAwards::kBreakMainHand,
                                   context.registry);
        }
    }
    Blocked answer{.amount = blocked, .pushed = false};
    if (!tagged(context, "is_projectile", damageType)) {
        answer.pushed = true;
        answer.thrown = attackerAnswers(session, source->entityId, context);
        const float seconds = secondsToDisable(source->entityId, context);
        if (seconds > 0.0F && context.disableBlocking) {
            context.disableBlocking(session, static_cast<std::int32_t>(std::lround(seconds * 20.0F)));
            playAround(session, "item.shield.break", 0.8F, context);
        }
    }
    return answer;
}

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

/// parity: `LivingEntity.knockback` as it lands on a player, plus the hurt animation. A blocked hit
/// first runs `blockedByItem`, whose half-block push points the other way and is halved by the
/// default one after it; a blocked hit also draws no hurt direction.
void pushAway(const PlayerSession& session, const entity::DamageSource& source, bool blocked) {
    const double towardsX = source.x - session.x;
    const double towardsZ = source.z - session.z;
    std::array<double, 3> start{0.0, 0.0, 0.0};
    if (blocked) {
        start = player::combat::knockback(kBlockedPush, -towardsX, -towardsZ, 0.0, 0.0, 0.0, session.onGround);
    }
    const std::array<double, 3> moved = player::combat::knockback(
        entity::entityCombat::kDefaultKnockback, towardsX, towardsZ, start[0], start[1], start[2], session.onGround);
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
    if (blocked) {
        return;
    }
    session.connection().sendPlayPacket(
        wire::protocol::play::HurtAnimationPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::HurtAnimationPacket{.entityId = session.entityId(), .yaw = angle}.write(buffer);
        });
}

/// A push the server decides on its own, sent as the player's new motion.
void shove(const PlayerSession& session, const entity::shieldBlows::Push& push) {
    if (session.connection().pushBack) {
        session.connection().pushBack(push.x, push.y, push.z, session.yaw);
        return;
    }
    if (!session.connection().sendPlayPacket) {
        return;
    }
    session.connection().sendPlayPacket(
        wire::protocol::play::SetEntityMotionPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::SetEntityMotionPacket{
                .entityId = session.entityId(), .velocityX = push.x, .velocityY = push.y, .velocityZ = push.z}
                .write(buffer);
        });
}

/// The hit as `DamagePredicate` reads it: before and after the shield, and who is behind it.
[[nodiscard]] world::TriggerDamage triggerDamageOf(float original, float taken, bool blocked,
                                                   const std::optional<Identifier>& damageType,
                                                   std::string_view deathMessageId,
                                                   const std::optional<entity::DamageSource>& source,
                                                   const DamageContext& context) {
    world::TriggerDamage hit{.dealt = original,
                             .taken = taken,
                             .blocked = blocked,
                             .type = damageType.value_or(world::triggerFacts::damageTypeOf(deathMessageId))};
    world::LoadedWorld* world = context.world ? context.world() : nullptr;
    if (!source.has_value() || world == nullptr) {
        return hit;
    }
    const std::vector<PlayerSession*> players =
        context.registry == nullptr ? std::vector<PlayerSession*>{} : context.registry->snapshot();
    const std::int32_t causing = source->ownerId.value_or(source->entityId);
    hit.source = world::triggerFacts::ofId(*world, players, causing);
    hit.direct = world::triggerFacts::ofId(*world, players, source->entityId);
    hit.directHit = causing == source->entityId;
    return hit;
}

} // namespace

float damageAfterProtection(const PlayerSession& session, const float amount,
                            const std::optional<Identifier>& damageType, const DamageContext& context,
                            float* resisted) {
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
        const float before = damage;
        damage = player::combat::afterResistance(damage, resistance->second.amplifier);
        if (resisted != nullptr) {
            *resisted = before - damage;
        }
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

bool shieldFaces(const PlayerSession& session, double fromX, double fromZ) {
    if (!session.blockingSlot.has_value() || session.blockingTicks < kShieldDelayTicks) {
        return false;
    }
    const std::optional<storage::StoredItemStack> shield = session.inventory.at(*session.blockingSlot);
    return shield.has_value() && shield->id == "minecraft:shield" && facesSource(session, fromX, fromZ);
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
    // parity: the shield takes its share before anything else looks at the hit.
    const Blocked blocked = blockWithShield(*session, incoming, damageType, source, context);
    incoming -= blocked.amount;
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
    float resisted = 0.0F;
    const float afterArmour = damageAfterProtection(*session, incoming, damageType, context, &resisted);
    // parity: `getDamageAfterMagicAbsorb` and `hurtServer`, both in tenths of a heart.
    if (resisted > 0.0F && resisted < kLargestCountedDamage) {
        statAwards::award(*session, "damage_resisted", std::lround(resisted * 10.0F));
    }
    if (blocked.amount > 0.0F && blocked.amount < kLargestCountedDamage) {
        statAwards::award(*session, "damage_blocked_by_shield", std::lround(blocked.amount * 10.0F));
    }
    if (!cooling) {
        // parity: `BlocksAttacks.onBlocked` plays the block instead of the damage event.
        if (blocked.amount > 0.0F) {
            playAround(*session, "item.shield.block", 1.0F, context);
        } else {
            showDamage(*session, damageType, source.has_value() ? source->entityId : -1, context);
        }
        // parity: a hit the shield took whole deals no knockback; only the attacker's own shove lands.
        const bool fullyBlocked = blocked.amount > 0.0F && incoming <= 0.0F;
        if (source.has_value() && !fullyBlocked) {
            pushAway(*session, *source, blocked.pushed);
        } else if (blocked.thrown.has_value()) {
            shove(*session, *blocked.thrown);
        }
    }
    std::optional<world::TriggerDamage> hit;
    if (context.triggers != nullptr) {
        hit = triggerDamageOf(amount, dealt, blocked.amount > 0.0F, damageType, deathMessageId, source, context);
        // parity: `hurtServer` ends in `ENTITY_HURT_PLAYER` whatever the shield and armour left.
        context.triggers->fire("entity_hurt_player", session->name(), session->uuid().toString(),
                               world::TriggerEvent{.damage = hit});
    }
    if (afterArmour <= 0.0F) {
        return;
    }
    // parity: `LivingEntity.hurt` takes it out of absorption first, and only the rest off health.
    const float absorbed = std::min(session->absorption, afterArmour);
    if (absorbed > 0.0F && absorbed < kLargestCountedDamage) {
        statAwards::award(*session, "damage_absorbed", std::lround(absorbed * 10.0F));
    }
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
    // parity: `resolvePlayerResponsibleForDamage`, a player's hit remembered for a hundred ticks,
    // and `setLastHurtByMob` for anything else that hit them.
    if (source.has_value() && context.registry != nullptr) {
        const std::int32_t responsible = source->ownerId.value_or(source->entityId);
        bool byPlayer = false;
        for (const PlayerSession* other : context.registry->snapshot()) {
            if (other->entityId() == responsible) {
                session->lastHurtByPlayerId = other->entityId();
                session->lastHurtByPlayerTime.store(kPlayerHitMemoryTicks);
                byPlayer = true;
                break;
            }
        }
        world::LoadedWorld* world = context.world ? context.world() : nullptr;
        const entity::ServerEntity* mob = byPlayer || world == nullptr ? nullptr : world->state().entity(responsible);
        if (mob != nullptr) {
            session->lastHurtByMobType = mob->type();
            session->lastHurtByMobTime.store(kPlayerHitMemoryTicks);
        }
    }
    session->stats.addDamageTaken(damage);
    // parity: `LivingEntity.hurtServer` raises `ENTITY_DAMAGE` right where the health comes off, so
    // absorption alone makes none. A player is an entity too and this is their half of that line.
    if (world::LoadedWorld* world = context.world ? context.world() : nullptr; world != nullptr) {
        world::vibrations::emit(world->state(), "entity_damage", session->x, session->y, session->z, world->gameTime());
    }
    session->setHealth(std::max(session->health() - damage, 0.0F));
    // parity: `getKillCredit().awardKillScore`: `ENTITY_KILLED_PLAYER` names the player who hit
    // them last, else the mob, whatever this blow was.
    if (session->health() <= 0.0F && hit.has_value()) {
        std::optional<::basalt::server::world::loot::EntityFacts> credit;
        world::LoadedWorld* world = context.world ? context.world() : nullptr;
        if (session->lastHurtByPlayerId.has_value() && session->lastHurtByPlayerTime.load() > 0 && world != nullptr &&
            context.registry != nullptr) {
            credit = world::triggerFacts::ofId(*world, context.registry->snapshot(), *session->lastHurtByPlayerId);
        } else if (session->lastHurtByMobType.has_value() && session->lastHurtByMobTime.load() > 0) {
            credit = hit->source.has_value() && hit->source->type == *session->lastHurtByMobType
                         ? hit->source
                         : ::basalt::server::world::loot::EntityFacts{.type = *session->lastHurtByMobType};
        }
        if (credit.has_value()) {
            context.triggers->fire("entity_killed_player", session->name(), session->uuid().toString(),
                                   world::TriggerEvent{.entities = {{"entity", *credit}}, .damage = hit});
        }
    }
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
