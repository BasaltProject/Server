#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ShieldBlows.hpp>
#include <basalt/server/runtime/item/ItemDamageEffects.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerAttributes.hpp>
#include <basalt/server/runtime/player/PlayerBlocksAttacks.hpp>
#include <basalt/server/runtime/player/PlayerCombatMessage.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerExhaustion.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/PlayerTotem.hpp>
#include <basalt/server/runtime/player/PlayerVersusPlayer.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/DamageProtection.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
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
#include <functional>
#include <numbers>
#include <random>
#include <variant>
#include <vector>

namespace basalt::server::runtime::player {
namespace {

/// parity: the `3.4028235E37F` every damage statistic stays under.
constexpr float kLargestCountedDamage = 3.4028235E37F;

/// parity: `setLastHurtByPlayer(player, 100)`.
constexpr std::int32_t kPlayerHitMemoryTicks = 100;

using wire::common::Identifier;

/// parity: `LivingEntity.invulnerableDuration` and the half inside.

constexpr double kRadiansToDegrees = 180.0 / std::numbers::pi;

[[nodiscard]] bool velocityAllowed(const PlayerSession& session, std::array<double, 3>& velocity) {
    if (session.events == nullptr) {
        return true;
    }
    const server::plugins::PluginEvent* answered = session.events->ask(
        &server::plugins::pluginEventTypes::playerVelocity, [&](server::plugins::PluginEvent& event) {
            plugins::EventSink::player(event, session);
            event.set("vx", velocity[0]);
            event.set("vy", velocity[1]);
            event.set("vz", velocity[2]);
        });
    if (answered == nullptr) {
        return true;
    }
    if (answered->cancelled()) {
        return false;
    }
    const auto edited = [&](const char* field, double& axis) {
        if (const auto* value = std::get_if<double>(&answered->get(field)); value != nullptr) {
            axis = *value;
        }
    };
    edited("vx", velocity[0]);
    edited("vy", velocity[1]);
    edited("vz", velocity[2]);
    return true;
}

/// parity: `blocks_attacks`.
constexpr float kBlockingAngleDegrees = 90.0F;
constexpr std::int32_t kDefaultBlockDelay = 5;
constexpr float kAxeDisableSeconds = 5.0F;
constexpr double kBlockedPush = 0.5;

/// parity: `Ravager.handleEntityEvent` 39, the stun.
constexpr std::int8_t kStunEvent = 39;

[[nodiscard]] Identifier parsedItem(std::string_view id) {
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(id);
    return parsed.has_value() ? *parsed : Identifier::vanilla("shield");
}

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

/// parity: `Weapon.disableBlockingForSeconds`, from the stack's own `weapon` else the axes' default.
[[nodiscard]] float weaponDisableSeconds(const storage::StoredItemStack& held) {
    if (const wire::nbt::NbtCompound* weapon = held.components.getCompound("minecraft:weapon"); weapon != nullptr) {
        return weapon->getFloat("disable_blocking_for_seconds", 0.0F);
    }
    return held.id.ends_with("_axe") ? kAxeDisableSeconds : 0.0F;
}

/// parity: `LivingEntity.getSecondsToDisableBlocking`.
[[nodiscard]] float secondsToDisable(std::int32_t attackerId, const DamageContext& context) {
    if (context.registry != nullptr) {
        for (const PlayerSession* attacker : context.registry->snapshot()) {
            if (attacker->entityId() != attackerId) {
                continue;
            }
            const std::optional<storage::StoredItemStack> held =
                attacker->inventory.at(PlayerInventory::kHotbarStart + attacker->inventory.heldSlot());
            return held.has_value() ? weaponDisableSeconds(*held) : 0.0F;
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

/// parity: `LivingEntity.applyItemBlocking`'s angle, straight behind when the source has no position.
[[nodiscard]] double angleToSource(const PlayerSession& session, const std::optional<entity::DamageSource>& source) {
    constexpr auto kRadians = static_cast<float>(std::numbers::pi / 180.0);
    if (!source.has_value()) {
        return static_cast<double>(static_cast<float>(std::numbers::pi));
    }
    const float yaw = -session.yaw * kRadians;
    const double viewX = static_cast<double>(wire::common::mth::sin(yaw));
    const double viewZ = static_cast<double>(wire::common::mth::cos(yaw));
    double toX = source->x - session.x;
    double toZ = source->z - session.z;
    const double length = std::sqrt((toX * toX) + (toZ * toZ));
    toX = length < 1.0E-5 ? 0.0 : toX / length;
    toZ = length < 1.0E-5 ? 0.0 : toZ / length;
    return std::acos((toX * viewX) + (toZ * viewZ));
}

[[nodiscard]] bool facesSource(const PlayerSession& session, double fromX, double fromZ) {
    constexpr auto kRadians = static_cast<float>(std::numbers::pi / 180.0);
    const entity::DamageSource at{.x = fromX, .z = fromZ};
    return angleToSource(session, at) <= static_cast<double>(kRadians * kBlockingAngleDegrees);
}

struct Blocked {
    float amount = 0.0F;
    bool pushed = false;
    std::optional<std::string> sound;
    std::optional<entity::shieldBlows::Push> thrown;
};

/// parity: `LivingEntity.blockUsingItem` hands the blow to the attacker's `blockedByItem`.
[[nodiscard]] entity::shieldBlows::Reply attackerAnswers(const PlayerSession& session, std::int32_t attackerId,
                                                         const DamageContext& context) {
    world::LoadedWorld* world = context.world ? context.world() : nullptr;
    entity::ServerEntity* attacker = world == nullptr ? nullptr : world->state().entity(attackerId);
    if (attacker == nullptr) {
        return {};
    }
    random::RandomSource random = random::RandomSource::legacy(static_cast<std::int64_t>(std::random_device{}()));
    const entity::shieldBlows::Reply reply = entity::shieldBlows::blocked(*attacker, session.x, session.z, 0.0, random);
    // parity: `Ravager.blockedByItem`'s stun.
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
    return reply;
}

/// parity: `LivingEntity.applyItemBlocking`, for whatever stack carries `blocks_attacks`.
[[nodiscard]] Blocked blockWithShield(PlayerSession& session, float damage, const std::optional<Identifier>& damageType,
                                      const std::optional<entity::DamageSource>& source, const DamageContext& context) {
    if (damage <= 0.0F || !session.blockingSlot.has_value()) {
        return {};
    }
    const std::int32_t slot = *session.blockingSlot;
    const std::optional<storage::StoredItemStack> shield = session.inventory.at(slot);
    const std::optional<blocksAttacks::Component> component =
        shield.has_value() ? blocksAttacks::of(*shield) : std::nullopt;
    if (!component.has_value()) {
        return {};
    }
    // parity: a plugin that set the delay keeps winning over the component's.
    const std::int32_t delay = session.extras.shieldBlockingDelay != kDefaultBlockDelay
                                   ? session.extras.shieldBlockingDelay
                                   : blocksAttacks::blockDelayTicks(*component);
    if (session.blockingTicks < delay ||
        (component->bypassedBy.has_value() &&
         blocksAttacks::contains(*component->bypassedBy, context.damageTypeTags, damageType))) {
        return {};
    }
    if (source.has_value() && source->piercing) {
        return {};
    }
    const float blocked = blocksAttacks::resolveBlockedDamage(*component, context.damageTypeTags, damageType, damage,
                                                              angleToSource(session, source));
    // parity: `BlocksAttacks.hurtBlockingItem`, every blocked hit before any wear.
    statAwards::used(session, shield->id);
    const std::int32_t wear = blocksAttacks::itemDamageFor(*component, blocked);
    if (wear > 0 && session.gamemode != wire::protocol::play::Gamemode::Creative) {
        random::RandomSource random = random::RandomSource::legacy(static_cast<std::int64_t>(std::random_device{}()));
        const item::ItemDamageEffects effects{[&context](const Identifier& id) {
            return context.enchantments == nullptr ? nullptr : context.enchantments->definitionOf(id);
        }};
        const std::optional<storage::StoredItemStack> worn = item::itemWear::hurt(
            *shield, wear, random,
            [&effects](const storage::StoredItemStack& stack, std::int32_t cost, random::RandomSource& source) {
                return effects.process(stack, cost, source);
            });
        world::triggerFacts::durabilityChanged(context.triggers, session, *shield, worn);
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
    Blocked answer{.amount = blocked, .pushed = false, .sound = component->blockSound};
    if (blocked > 0.0F && source.has_value() && !tagged(context, "is_projectile", damageType)) {
        const entity::shieldBlows::Reply reply = attackerAnswers(session, source->entityId, context);
        answer.pushed = !reply.push.has_value() && !reply.stunned;
        answer.thrown = reply.push;
        const std::optional<storage::StoredItemStack> still = session.inventory.at(slot);
        const std::optional<blocksAttacks::Component> blocking =
            still.has_value() && session.blockingSlot.has_value() ? blocksAttacks::of(*still) : std::nullopt;
        const float seconds = blocking.has_value() ? secondsToDisable(source->entityId, context) : 0.0F;
        const std::int32_t ticks = blocking.has_value() ? blocksAttacks::disableTicks(*blocking, seconds) : 0;
        std::int32_t disabled = ticks;
        if (disabled > 0 && session.events != nullptr) {
            const server::plugins::PluginEvent* answered = session.events->ask(
                &server::plugins::pluginEventTypes::playerShieldDisable, [&](server::plugins::PluginEvent& event) {
                    plugins::EventSink::player(event, session);
                    event.set("damager_id", static_cast<std::int64_t>(source->entityId));
                    event.set("ticks", static_cast<std::int64_t>(disabled));
                });
            if (answered != nullptr) {
                disabled = answered->cancelled() ? 0 : disabled;
                if (const auto* edited = std::get_if<std::int64_t>(&answered->get("ticks"));
                    edited != nullptr && disabled > 0) {
                    disabled = static_cast<std::int32_t>(*edited);
                }
            }
        }
        if (disabled > 0 && context.disableBlocking) {
            context.disableBlocking(session, parsedItem(still->id), disabled);
            if (blocking->disableSound.has_value()) {
                playAround(session, *blocking->disableSound, 0.8F, context);
            }
        }
    }
    return answer;
}

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

/// parity: `LivingEntity.knockback`.
void pushAway(PlayerSession& session, const entity::DamageSource& source, bool blocked, double resistance) {
    const double towardsX = source.x - session.x;
    const double towardsZ = source.z - session.z;
    // parity: the push starts from what the server still has of the player's motion.
    std::array<double, 3> start = session.deltaMovement;
    const std::int32_t direct = source.entityId;
    const std::int32_t attacker = source.ownerId.value_or(direct);
    const auto knock = [&](entity::entityCombat::Knock push) {
        const auto deltaOf = [&](const entity::entityCombat::Knock& aimed) {
            const std::array<double, 3> after =
                player::combat::knockback(aimed.power, aimed.towardsX, aimed.towardsZ, start[0], start[1], start[2],
                                          session.onGround, resistance);
            return std::array<double, 3>{after[0] - start[0], after[1] - start[1], after[2] - start[2]};
        };
        push.delta = deltaOf(push);
        if (!knockAllowed(session, push)) {
            return;
        }
        if (push.recompute) {
            push.delta = deltaOf(push);
        }
        for (std::size_t axis = 0; axis < start.size(); ++axis) {
            start[axis] += push.delta[axis];
        }
    };
    // parity: `LivingEntity.blockedByItem` then `dealDefaultKnockback`, one event each.
    if (blocked) {
        knock({.cause = "shield_block",
               .sourceId = attacker,
               .power = kBlockedPush,
               .towardsX = -towardsX,
               .towardsZ = -towardsZ});
    }
    knock({.cause = "entity_attack",
           .sourceId = direct,
           .power = entity::entityCombat::kDefaultKnockback,
           .towardsX = towardsX,
           .towardsZ = towardsZ});
    std::array<double, 3> moved = start;
    if (source.punch[0] != 0.0 || source.punch[1] != 0.0) {
        const double kept = std::max(0.0, 1.0 - resistance);
        entity::entityCombat::Knock punch{.cause = "push",
                                          .sourceId = direct,
                                          .power = std::hypot(source.punch[0], source.punch[1]) * kept,
                                          .towardsX = -source.punch[0],
                                          .towardsZ = -source.punch[1],
                                          .delta = {source.punch[0] * kept, 0.1, source.punch[1] * kept}};
        if (knockAllowed(session, punch)) {
            moved[0] += punch.delta[0];
            moved[1] += punch.delta[1];
            moved[2] += punch.delta[2];
        }
    }
    if (!velocityAllowed(session, moved)) {
        return;
    }
    session.deltaMovement = moved;
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

void shove(PlayerSession& session, const entity::shieldBlows::Push& push) {
    std::array<double, 3> pushed{push.x, push.y, push.z};
    if (!velocityAllowed(session, pushed)) {
        return;
    }
    session.deltaMovement = pushed;
    if (session.connection().pushBack) {
        session.connection().pushBack(pushed[0], pushed[1], pushed[2], session.yaw);
        return;
    }
    if (!session.connection().sendPlayPacket) {
        return;
    }
    session.connection().sendPlayPacket(
        wire::protocol::play::SetEntityMotionPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::SetEntityMotionPacket{
                .entityId = session.entityId(), .velocityX = pushed[0], .velocityY = pushed[1], .velocityZ = pushed[2]}
                .write(buffer);
        });
}

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

[[nodiscard]] TotemContext totemContextOf(const DamageContext& context, bool bypassesInvulnerability) {
    return TotemContext{
        .registry = context.registry,
        .bypassesInvulnerability = bypassesInvulnerability,
        .worksAnywhereInInventory = context.world && context.world() != nullptr &&
                                    context.world()->state().gameplay().player.totemOfUndyingWorksInInventory,
        .nextFloat =
            [] {
                random::RandomSource random =
                    random::RandomSource::legacy(static_cast<std::int64_t>(std::random_device{}()));
                return random.nextFloat();
            },
        .slotChanged = context.slotChanged,
        .usedTotem =
            [triggers = context.triggers](PlayerSession& player, const storage::StoredItemStack& spent) {
                const std::optional<world::TriggerItem> item = world::triggerFacts::itemOf(spent);
                if (triggers != nullptr && item.has_value()) {
                    triggers->fire("used_totem", player.name(), player.uuid().toString(),
                                   world::TriggerEvent{.items = {{"item", *item}}});
                }
            },
        .resurrectAllowed = context.resurrectAllowed,
        .interactFinished =
            [world = context.world, triggers = context.triggers](const PlayerSession& player) {
                if (world::LoadedWorld* here = world ? world() : nullptr; here != nullptr) {
                    world::vibrations::emit(here->state(), "item_interact_finish", player.x, player.y, player.z,
                                            here->gameTime(), world::triggerFacts::vibrationSourceOf(player, triggers));
                }
            }};
}

[[nodiscard]] playerAttributes::DefinitionOf definitionsOf(const DamageContext& context) {
    world::RealEnchantments* enchantments = context.enchantments;
    return [enchantments](const Identifier& id) {
        return enchantments == nullptr ? nullptr : enchantments->definitionOf(id);
    };
}

} // namespace

float damageAfterProtection(const PlayerSession& session, const float amount,
                            const std::optional<Identifier>& damageType, const DamageContext& context, float* resisted,
                            float armorFractionAdd) {
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
        // parity: `CombatRules.getDamageAfterAbsorb` with `getArmorValue` floored and toughness as is.
        const playerAttributes::DefinitionOf definitionOf = definitionsOf(context);
        const playerAttributes::Modifiers modifiers = playerAttributes::modifiersOf(session, definitionOf);
        const auto attribute = [&session, &modifiers](std::string_view name) {
            const auto found = modifiers.find(name);
            return playerAttributes::calculate(name, playerAttributes::baseOf(session, name),
                                               found == modifiers.end() ? std::vector<playerAttributes::Modifier>{}
                                                                        : found->second);
        };
        const auto armour = static_cast<float>(std::floor(attribute("minecraft:armor")));
        const auto toughness = static_cast<float>(attribute("minecraft:armor_toughness"));
        if (armour > 0.0F) {
            damage = player::combat::afterArmour(damage, armour, toughness, armorFractionAdd);
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

/// parity: `FallLocation.getCurrentFallLocation`, from the block the player stands in.
[[nodiscard]] std::string fallLocationAt(const PlayerSession& session, world::LoadedWorld& world) {
    const std::optional<std::int32_t> state = world.state().blockState(
        static_cast<std::int32_t>(std::floor(session.x)), static_cast<std::int32_t>(std::floor(session.y)),
        static_cast<std::int32_t>(std::floor(session.z)));
    if (!state.has_value()) {
        return {};
    }
    const Identifier* block = server::registry::blockStateDefinitions::blockOf(*state);
    if (block == nullptr) {
        return {};
    }
    const std::string_view path = block->path();
    if (path == "ladder") {
        return "ladder";
    }
    if (path == "vine") {
        return "vines";
    }
    if (path == "weeping_vines" || path == "weeping_vines_plant") {
        return "weeping_vines";
    }
    if (path == "twisting_vines" || path == "twisting_vines_plant") {
        return "twisting_vines";
    }
    if (path == "scaffolding") {
        return "scaffolding";
    }
    if (path == "cave_vines" || path == "cave_vines_plant") {
        return "other_climbable";
    }
    return world::fluids::of(*state) == world::Submersion::Water ? "water" : std::string{};
}

bool shieldFaces(const PlayerSession& session, double fromX, double fromZ) {
    if (!session.blockingSlot.has_value()) {
        return false;
    }
    const std::optional<storage::StoredItemStack> shield = session.inventory.at(*session.blockingSlot);
    const std::optional<blocksAttacks::Component> component =
        shield.has_value() ? blocksAttacks::of(*shield) : std::nullopt;
    const std::int32_t delay = session.extras.shieldBlockingDelay != kDefaultBlockDelay
                                   ? session.extras.shieldBlockingDelay
                                   : (component.has_value() ? blocksAttacks::blockDelayTicks(*component) : 0);
    return component.has_value() && session.blockingTicks >= delay && facesSource(session, fromX, fromZ);
}

void hurtPlayer(PlayerSession& player, const float amount, const std::string_view deathMessageId,
                const std::optional<Identifier>& damageType, const std::optional<entity::DamageSource>& source,
                const DamageContext& context) {
    PlayerSession* session = &player;
    session->lastHurtLanded = false;
    if (session->dead || (session->invulnerable() && !tagged(context, "bypasses_invulnerability", damageType))) {
        return;
    }
    // parity: `ServerPlayer.hurtServer`.
    if (source.has_value() && context.registry != nullptr) {
        const std::int32_t responsible = source->ownerId.value_or(source->entityId);
        const world::LoadedWorld* world = context.world ? context.world() : nullptr;
        for (const PlayerSession* attacker : context.registry->snapshot()) {
            if (attacker->entityId() == responsible &&
                !pvp::canHarm(*session, *attacker, world == nullptr || world->gameRules().get(world::gameRules::kPvp),
                              context.teams)) {
                return;
            }
        }
    }
    // parity: `Player.hurtServer` throws off whatever sits on the shoulders.
    session->shoulderDropPending = true;
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
    // parity: `LivingEntity.hurt`'s cooldown.
    const float dealt = incoming;
    const std::int32_t hurtCooldown = session->extras.maxNoDamageTicks;
    const bool cooling = session->invulnerableTicks * 2 > hurtCooldown;
    if (cooling) {
        if (dealt <= session->lastHurt) {
            return;
        }
        incoming = dealt - session->lastHurt;
        session->lastHurt = dealt;
    } else {
        session->lastHurt = dealt;
        session->invulnerableTicks = hurtCooldown;
    }
    // parity: `hurtServer` answers `!blocked || damage > 0`.
    session->lastHurtLanded = blocked.amount <= 0.0F || incoming > 0.0F;
    // parity: `AbstractArrow.onHitEntity` leaves a non-piercing arrow in whoever it really hurt.
    if (damageType == world::damageTypes::kArrow && source.has_value() && !source->piercing &&
        (blocked.amount <= 0.0F || incoming > 0.0F)) {
        ++session->arrowCount;
    }
    float resisted = 0.0F;
    const float afterArmour = damageAfterProtection(*session, incoming, damageType, context, &resisted,
                                                    source.has_value() ? source->armorFractionAdd : 0.0F);
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
            if (blocked.sound.has_value()) {
                playAround(*session, *blocked.sound, 1.0F, context);
            }
        } else {
            showDamage(*session, damageType, source.has_value() ? source->entityId : -1, context);
        }
        // parity: `hurtServer`.
        const bool fullyBlocked = blocked.amount > 0.0F && incoming <= 0.0F;
        if (source.has_value() && !fullyBlocked) {
            if (!tagged(context, "no_knockback", damageType)) {
                pushAway(*session, *source, blocked.pushed,
                         playerAttributes::valueOf(*session, "minecraft:knockback_resistance", definitionsOf(context)));
            }
            if (blocked.thrown.has_value()) {
                shove(*session, *blocked.thrown);
            }
        } else if (blocked.thrown.has_value()) {
            shove(*session, *blocked.thrown);
        }
    }
    std::optional<world::TriggerDamage> hit;
    if (context.triggers != nullptr) {
        hit = triggerDamageOf(amount, dealt, blocked.amount > 0.0F, damageType, deathMessageId, source, context);
    }

    // parity: `hurtServer` ends in `ENTITY_HURT_PLAYER` whatever the shield.
    struct ScopeExit {
        std::function<void()> run;

        ~ScopeExit() { run(); }
    } const hurtTrigger{[&context, session, &hit, &source] {
        if (context.triggers == nullptr) {
            return;
        }
        context.triggers->fire("entity_hurt_player", session->name(), session->uuid().toString(),
                               world::TriggerEvent{.damage = hit});
        // parity: `PLAYER_HURT_ENTITY` for the player behind the hit.
        if (!source.has_value() || context.registry == nullptr || !context.triggers->listensTo("player_hurt_entity")) {
            return;
        }
        const std::int32_t responsible = source->ownerId.value_or(source->entityId);
        for (const PlayerSession* attacker : context.registry->snapshot()) {
            if (attacker->entityId() == responsible) {
                context.triggers->fire(
                    "player_hurt_entity", attacker->name(), attacker->uuid().toString(),
                    world::TriggerEvent{.entities = {{"entity", world::triggerFacts::ofPlayer(*session)}},
                                        .damage = hit});
                return;
            }
        }
    }};

    if (afterArmour <= 0.0F) {
        return;
    }
    // parity: `LivingEntity.hurt` takes it out of absorption first.
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
    // parity: `resolvePlayerResponsibleForDamage`.
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
    // parity: `Player.actuallyHurt` charges the damage type's exhaustion once the hit got through.
    causeExhaustion(*session, damageExhaustion(damageType.value_or(world::triggerFacts::damageTypeOf(deathMessageId))),
                    "damaged");
    session->stats.addDamageTaken(damage);
    {
        world::LoadedWorld* recorded = context.world ? context.world() : nullptr;
        // parity: `CombatTracker.recheckStatus` runs before each hit is recorded.
        if (recorded != nullptr && !session->combatRecords.empty() &&
            recorded->gameTime() - session->combatRecords.back().tick > (session->inCombat ? 300 : 100)) {
            session->combatRecords.clear();
            session->inCombat = false;
        }
        if (session->combatRecords.size() >= PlayerSession::kMaxCombatRecords) {
            session->combatRecords.erase(session->combatRecords.begin());
        }
        session->combatRecords.push_back(PlayerSession::CombatRecord{
            .damageType = damageType.value_or(world::triggerFacts::damageTypeOf(deathMessageId)).toString(),
            .messageId = std::string{deathMessageId},
            .directId = source.has_value() ? source->entityId : -1,
            .causeId = source.has_value() ? source->ownerId.value_or(source->entityId) : -1,
            .damage = damage,
            .fallDistance = static_cast<float>(session->fallDistance),
            .fallLocation = recorded == nullptr ? std::string{} : fallLocationAt(*session, *recorded),
            .tick = recorded == nullptr ? 0 : recorded->gameTime()});
        const std::int32_t cause = session->combatRecords.back().causeId;
        if (!session->inCombat && cause >= 0 && context.death.labelOf && context.death.labelOf(cause).has_value()) {
            session->inCombat = true;
        }
    }
    // parity: `LivingEntity.hurtServer` raises `ENTITY_DAMAGE`.
    if (world::LoadedWorld* world = context.world ? context.world() : nullptr; world != nullptr) {
        world::vibrations::emit(world->state(), "entity_damage", session->x, session->y, session->z, world->gameTime());
    }
    session->setHealth(std::max(session->health() - damage, 0.0F));
    if (session->health() <= 0.0F) {
        const Identifier type = damageType.value_or(world::triggerFacts::damageTypeOf(deathMessageId));
        const bool bypasses = context.damageTypeTags != nullptr &&
                              world::realDamageTypeTags::isIn(
                                  *context.damageTypeTags, world::realDamageTypeTags::kBypassesInvulnerability, type);
        if (checkTotemDeathProtection(*session, totemContextOf(context, bypasses))) {
            return;
        }
    }
    // parity: `getKillCredit().awardKillScore`.
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
        killPlayer(*session, deathMessageId, source.has_value() ? source->name : std::nullopt, context.death,
                   trackedDeathMessage(*session, deathMessageId, source, context.death));
    } else if (session->connection().syncHealth) {
        session->connection().syncHealth();
    }
}

void healPlayer(PlayerSession& player, const float amount, const DamageContext& context, const std::string_view cause,
                const bool fast) {
    PlayerSession* session = &player;
    if (session->dead) {
        return;
    }
    float healed = amount;
    if (context.healed) {
        const std::optional<float> allowed = context.healed(*session, amount, cause, fast);
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

bool knockAllowed(const PlayerSession& session, entity::entityCombat::Knock& knock) {
    if (session.events == nullptr) {
        return true;
    }
    const server::plugins::PluginEvent* answered = session.events->ask(
        &server::plugins::pluginEventTypes::entityKnockback, [&](server::plugins::PluginEvent& event) {
            event.set("entity_id", static_cast<std::int64_t>(session.entityId()));
            event.set("entity_type", std::string{"minecraft:player"});
            event.set("world", session.worldName);
            plugins::hookEvents::fillKnock(event, knock);
        });
    return plugins::hookEvents::readKnock(answered, knock);
}

} // namespace basalt::server::runtime::player
