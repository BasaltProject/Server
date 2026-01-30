#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/DamageProtection.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>

namespace basalt::server::runtime {
namespace {

using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::Gamemode;
using wire::text::Component;

/// parity: `LivingEntity.invulnerableDuration`, and the half of it inside which only a bigger hit
/// lands.
constexpr std::int32_t kHurtCooldownTicks = 20;
constexpr std::int32_t kCooldownHalfTicks = 10;

/// parity: `Entity.getEyeHeight` and the lift a dismount gives.
constexpr double kDismountLift = 0.5;

constexpr double kRadiansToDegrees = 180.0 / std::numbers::pi;

/// The blocks that cancel a fall while you are in them. parity: `Entity.onClimbable`.
const std::set<std::string, std::less<>>& climbableBlocks() {
    static const std::set<std::string, std::less<>> kBlocks{"ladder",
                                                            "vine",
                                                            "scaffolding",
                                                            "twisting_vines",
                                                            "twisting_vines_plant",
                                                            "weeping_vines",
                                                            "weeping_vines_plant",
                                                            "cave_vines",
                                                            "cave_vines_plant"};
    return kBlocks;
}

} // namespace

CombatHandler::CombatHandler(ClientSession& owner, Hooks hooks, Options options)
    : owner_(&owner), hooks_(std::move(hooks)), options_(std::move(options)) {}

void CombatHandler::dismount(PlayerSession& session) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    entity::ServerEntity* vehicle = session.vehicleId.has_value() ? world->state().entity(*session.vehicleId) : nullptr;
    session.vehicleId.reset();
    if (vehicle == nullptr) {
        return;
    }
    std::erase(vehicle->passengerIds, session.entityId());
    const wire::protocol::play::SetPassengersPacket packet{.vehicleId = vehicle->entityId(),
                                                           .passengerIds = vehicle->passengerIds};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::SetPassengersPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
    // The client puts itself back on the ground, but the server's own copy of where they are came
    // from the cart, so it is told the same thing rather than left a tick behind.
    owner_->teleportTo(session.x, session.y + kDismountLift, session.z, session.yaw, session.pitch);
}

float CombatHandler::reduced(const PlayerSession& session, float amount,
                             const std::optional<Identifier>& damageType) const {
    const world::realDamageTypeTags::TagMembers* tags = owner_->options().damageTypeTags;
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
    world::RealEnchantments* enchantments = owner_->options().enchantments;
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

void CombatHandler::trackFall(PlayerSession& session, double y, bool onGround,
                              const std::optional<std::int32_t>& feetState) {
    const bool climbing = feetState.has_value() && [&] {
        const std::optional<server::registry::BlockState> block = server::registry::blocks::byStateId(*feetState);
        return block.has_value() && climbableBlocks().contains(block->block.path());
    }();
    const bool cancelled =
        session.flying || session.gamemode == Gamemode::Creative || session.gamemode == Gamemode::Spectator ||
        (feetState.has_value() && (world::fluids::of(*feetState) != world::Submersion::None || climbing));
    if (cancelled) {
        fallDistance = 0.0;
        return;
    }
    if (!onGround && y < session.y) {
        fallDistance += session.y - y;
    }
    if (onGround && !session.onGround) {
        // parity: `Entity.checkFallDamage` raises `HIT_GROUND` at the landing whenever anything was
        // fallen, before the damage and whatever the fall-damage rule says. A landing that this build
        // cancelled the fall for (flying, creative, water, a ladder) has nothing counted and so makes
        // no event, where the jar counts the distance for everyone and only spares the damage.
        if (fallDistance > 0.0) {
            if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
                world::vibrations::emit(world->state(), "hit_ground", session.x, y, session.z, world->gameTime());
            }
        }
        applyFallDamage(session);
        fallDistance = 0.0;
    }
    if (onGround) {
        fallDistance = 0.0;
    }
}

void CombatHandler::applyFallDamage(PlayerSession& session) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world != nullptr && !world->gameRules().get(world::gameRules::kFallDamage)) {
        return;
    }
    const std::int32_t damage = player::combat::fallDamage(fallDistance);
    if (damage <= 0) {
        return;
    }
    session.stats.addFallen(fallDistance);
    owner_->hurt(static_cast<float>(damage), "fall", world::damageTypes::kFall);
}

void CombatHandler::sendDamageEvent(const PlayerSession& session, const std::optional<Identifier>& damageType,
                                    std::int32_t causeEntityId) {
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
    const wire::protocol::play::DamageEventPacket packet{.entityId = session.entityId(),
                                                         .damageType = registryId,
                                                         .causeEntityId = causeEntityId,
                                                         .directEntityId = causeEntityId};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::DamageEventPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void CombatHandler::pushAway(const PlayerSession& session, const entity::DamageSource& source) {
    const double towardsX = source.x - session.x;
    const double towardsZ = source.z - session.z;
    const std::array<double, 3> moved = player::combat::knockback(entity::entityCombat::kDefaultKnockback, towardsX,
                                                                  towardsZ, 0.0, 0.0, 0.0, session.onGround);
    owner_->sendPlayPacket(wire::protocol::play::SetEntityMotionPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::SetEntityMotionPacket{
            .entityId = session.entityId(), .velocityX = moved[0], .velocityY = moved[1], .velocityZ = moved[2]}
            .write(buffer);
    });
    const auto angle = static_cast<float>(std::atan2(towardsZ, towardsX) * kRadiansToDegrees) - session.yaw;
    owner_->sendPlayPacket(wire::protocol::play::HurtAnimationPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::HurtAnimationPacket{.entityId = session.entityId(), .yaw = angle}.write(buffer);
    });
}

void CombatHandler::hurt(float amount, std::string_view deathMessageId, const std::optional<Identifier>& damageType,
                         const std::optional<entity::DamageSource>& source) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || session->dead || session->invulnerable()) {
        return;
    }
    // A player is an entity too, so it is the same hook; what changes is that the choke point for a
    // player is here rather than in the entity's own combat.
    float incoming = amount;
    if (hooks_.damaged) {
        const std::optional<float> allowed =
            hooks_.damaged(*session, amount, damageType.has_value() ? damageType->path() : deathMessageId,
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
    const float afterArmour = reduced(*session, incoming, damageType);
    if (afterArmour <= 0.0F) {
        return;
    }
    if (!cooling) {
        sendDamageEvent(*session, damageType, source.has_value() ? source->entityId : -1);
        if (source.has_value()) {
            pushAway(*session, *source);
        }
    }
    // parity: `LivingEntity.hurt` takes it out of absorption first, and only the rest off health.
    const float absorbed = std::min(session->absorption, afterArmour);
    if (absorbed > 0.0F) {
        session->absorption -= absorbed;
        owner_->uiState().syncAbsorption();
    }
    const float damage = afterArmour - absorbed;
    if (damage <= 0.0F) {
        return;
    }
    session->stats.addDamageTaken(damage);
    // parity: `LivingEntity.hurtServer` raises `ENTITY_DAMAGE` right where the health comes off, so
    // absorption alone makes none. A player is an entity too and this is their half of that line.
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        world::vibrations::emit(world->state(), "entity_damage", session->x, session->y, session->z, world->gameTime());
    }
    session->setHealth(std::max(session->health() - damage, 0.0F));
    if (session->health() <= 0.0F) {
        die(deathMessageId, source.has_value() ? source->name : std::nullopt);
    } else {
        owner_->uiState().syncHealth();
    }
}

void CombatHandler::heal(float amount) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || session->dead) {
        return;
    }
    float healed = amount;
    if (hooks_.healed) {
        const std::optional<float> allowed = hooks_.healed(*session, amount);
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
    owner_->uiState().syncHealth();
}

void CombatHandler::killPlayer(std::string_view deathMessageId) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || session->dead) {
        return;
    }
    session->setHealth(0.0F);
    die(deathMessageId, std::nullopt);
}

void CombatHandler::die(std::string_view deathMessageId, const std::optional<Component>& killer) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    session->dead = true;
    session->setHealth(0.0F);
    // parity: `LivingEntity.die` empties the absorption pool, so respawning starts without it.
    session->absorption = 0.0F;
    session->stats.increment(storage::stats::deaths());
    session->stats.reset(storage::stats::timeSinceRest());
    owner_->uiState().syncHealth();
    // parity: `DamageSource.getLocalizedDeathMessage`, whose second argument is whoever did it.
    // Half of vanilla's death messages name them, and one built without it reads as `%2$s`.
    std::vector<Component> arguments{Component::text(session->name())};
    if (killer.has_value()) {
        arguments.push_back(*killer);
    }
    const Component message = Component::translatable("death.attack." + std::string{deathMessageId}, arguments);
    if (hooks_.died) {
        hooks_.died(*session, wire::text::resolvePlainText(message, owner_->consoleLang()));
    }
    owner_->sendCompressed(wire::protocol::play::PlayerCombatKillPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::PlayerCombatKillPacket{.playerId = session->entityId(), .message = message}.write(buffer);
    });
    owner_->logger().info([&] { return wire::text::resolvePlainText(message, owner_->consoleLang()); });
    player::PlayerRegistry* registry = owner_->options().playerRegistry;
    if (registry == nullptr) {
        return;
    }
    for (PlayerSession* player : registry->snapshot()) {
        if (!player->connection().sendPlayPacket) {
            continue;
        }
        player->connection().sendPlayPacket(
            wire::protocol::play::SystemChatPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                wire::protocol::play::SystemChatPacket{.content = message, .isActionBar = false}.write(buffer);
            });
    }
}

} // namespace basalt::server::runtime
