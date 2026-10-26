#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ShieldBlows.hpp>
#include <basalt/server/runtime/item/ItemDamageEffects.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerAttributes.hpp>
#include <basalt/server/runtime/player/PlayerCombatMessage.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/PlayerTotem.hpp>
#include <basalt/server/runtime/player/PlayerVersusPlayer.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
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

/// parity: `LivingEntity.getSecondsToDisableBlocking`.
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
    bool pushed = false;
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
    return reply.push;
}

/// parity: `LivingEntity.applyItemBlocking` for a shield.
[[nodiscard]] Blocked blockWithShield(PlayerSession& session, float damage, const std::optional<Identifier>& damageType,
                                      const std::optional<entity::DamageSource>& source, const DamageContext& context) {
    if (damage <= 0.0F || !session.blockingSlot.has_value() ||
        session.blockingTicks < session.extras.shieldBlockingDelay) {
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
    // parity: `ItemDamageFunction`.
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
    if (blocked) {
        start = player::combat::knockback(kBlockedPush, -towardsX, -towardsZ, start[0], start[1], start[2],
                                          session.onGround, resistance);
    }
    std::array<double, 3> moved = player::combat::knockback(entity::entityCombat::kDefaultKnockback, towardsX, towardsZ,
                                                            start[0], start[1], start[2], session.onGround, resistance);
    if (source.punch[0] != 0.0 || source.punch[1] != 0.0) {
        const double kept = std::max(0.0, 1.0 - resistance);
        moved[0] += source.punch[0] * kept;
        moved[1] += 0.1;
        moved[2] += source.punch[1] * kept;
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
    if (!session.blockingSlot.has_value() || session.blockingTicks < session.extras.shieldBlockingDelay) {
        return false;
    }
    const std::optional<storage::StoredItemStack> shield = session.inventory.at(*session.blockingSlot);
    return shield.has_value() && shield->id == "minecraft:shield" && facesSource(session, fromX, fromZ);
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
            playAround(*session, "item.shield.block", 1.0F, context);
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

} // namespace basalt::server::runtime::player
