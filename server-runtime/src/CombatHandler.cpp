#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerCooldowns.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerDeath.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/Cushions.hpp>
#include <basalt/server/runtime/world/DamageProtection.hpp>
#include <basalt/server/runtime/world/FallLanding.hpp>
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
#include <utility>

namespace basalt::server::runtime {
namespace {

using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::Gamemode;
using wire::text::Component;

/// parity: `Entity.getEyeHeight` and the lift a dismount gives.
constexpr double kDismountLift = 0.5;

/// parity: `LivingEntity.getEffectiveGravity`, the least fall speed that bounces, and
/// `getBlockPosBelowThatAffectsMyMovement`'s depth.
constexpr double kBounceGravity = 0.08;
constexpr double kBounceProbeDepth = 0.500001;

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
    // parity: `Cushion.removePassenger`, whose sound is only for a cushion still standing.
    if (world::cushions::isCushion(vehicle->type()) && !vehicle->removed) {
        owner_->broadcastSound("minecraft:entity.cushion.get_up", vehicle->x, vehicle->y, vehicle->z,
                               wire::protocol::play::SoundSource::Neutral);
    }
    const wire::protocol::play::SetPassengersPacket packet{.vehicleId = vehicle->entityId(),
                                                           .passengerIds = vehicle->passengerIds};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPassengers) {
            player->connection().sendPassengers(vehicle->entityId(), vehicle->passengerIds);
            continue;
        }
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::SetPassengersPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
    // The client puts itself back on the ground, but the server's own copy of where they are came
    // from the cart, so it is told the same thing rather than left a tick behind.
    owner_->teleportTo(session.x, session.y + kDismountLift, session.z, session.yaw, session.pitch);
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
        resetFall(session);
        return;
    }
    if (!onGround && y < session.y) {
        fallDistance += session.y - y;
    }
    // parity: `trackStartFallingPosition`, where the player is once any fall is counted.
    if (fallDistance > 0.0 && !fallStart.has_value()) {
        fallStart = std::array<double, 3>{session.x, y, session.z};
    }
    if (onGround && !session.onGround) {
        // parity: `Entity.checkFallDamage` raises `HIT_GROUND` at the landing whenever anything was
        // fallen, before the damage and whatever the fall-damage rule says. A landing that this build
        // cancelled the fall for (flying, creative, water, a ladder) has nothing counted and so makes
        // no event, where the jar counts the distance for everyone and only spares the damage.
        if (fallDistance > 0.0) {
            if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
                world::vibrations::emit(world->state(), "hit_ground", session.x, y, session.z, world->gameTime(),
                                        {.entity = session.uuid()});
                bounceOn(session, *world, y);
            }
        }
        applyFallDamage(session, y);
        resetFall(session);
    }
    if (onGround) {
        resetFall(session);
    }
}

void CombatHandler::resetFall(const PlayerSession& session) {
    world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
    if (session.health() > 0.0F && fallStart.has_value() && triggers != nullptr) {
        triggers->fire("fall_from_height", session.name(), session.uuid().toString(),
                       world::TriggerEvent{.startPosition = fallStart});
    }
    fallStart.reset();
    fallDistance = 0.0;
}

void CombatHandler::bounceOn(PlayerSession& session, world::LoadedWorld& world, double landingY) {
    // parity: `restituteMovementAfterCollisions` bounces only a fall faster than gravity, never a
    // sneaking one; of what bounces, only `ShelfMushroomBlock.bounceOn` makes a sound.
    if (session.sneaking || fallDistance <= kBounceGravity) {
        return;
    }
    const auto blockX = static_cast<std::int32_t>(std::floor(session.x));
    const auto blockY = static_cast<std::int32_t>(std::floor(landingY - kBounceProbeDepth));
    const auto blockZ = static_cast<std::int32_t>(std::floor(session.z));
    const std::optional<std::int32_t> under = world.state().blockState(blockX, blockY, blockZ);
    const std::optional<server::registry::BlockState> block =
        under.has_value() ? server::registry::blocks::byStateId(*under) : std::nullopt;
    if (!block.has_value() || block->block.path() != "shelf_mushroom" ||
        server::registry::blockMovement::bouncinessOf(*under) <= 0.0F) {
        return;
    }
    const std::int64_t seed = options_.nextSeed ? options_.nextSeed() : 0;
    for (PlayerSession* listener : owner_->playersInSameWorld()) {
        player::playSoundFor(*listener, "minecraft:block.shelf_mushroom.bounce",
                             wire::protocol::play::SoundSource::Blocks, blockX + 0.5, blockY + 0.5, blockZ + 0.5, 1.0F,
                             1.0F, seed);
    }
}

void CombatHandler::applyFallDamage(PlayerSession& session, const double landingY) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world != nullptr && !world->gameRules().get(world::gameRules::kFallDamage)) {
        return;
    }
    // parity: `Entity.checkFallDamage` hands the fall to the block under the feet, `Block.fallOn`.
    const std::optional<std::int32_t> under =
        world == nullptr ? std::nullopt
                         : world->state().blockState(
                               static_cast<std::int32_t>(std::floor(session.x)),
                               static_cast<std::int32_t>(std::floor(landingY - world::fallLanding::kProbeDepth)),
                               static_cast<std::int32_t>(std::floor(session.z)));
    const world::fallLanding::Landing landing =
        under.has_value() ? world::fallLanding::of(*under, session.sneaking) : world::fallLanding::Landing{};
    const std::int32_t damage = world::fallLanding::damage(fallDistance, landing, player::combat::kSafeFallDistance);
    if (damage <= 0) {
        return;
    }
    session.stats.addFallen(fallDistance * landing.distanceScale + landing.extraDistance);
    if (landing.stalagmite) {
        owner_->hurt(static_cast<float>(damage), "stalagmite", world::damageTypes::kStalagmite);
        return;
    }
    owner_->hurt(static_cast<float>(damage), "fall", world::damageTypes::kFall);
}

/// The context the edition-agnostic damage needs, built from what this connection knows.
player::DamageContext CombatHandler::damageContext() const {
    player::DamageContext context;
    context.registry = owner_->options().playerRegistry;
    context.triggers = owner_->options().advancementTriggers;
    context.damageTypeTags = owner_->options().damageTypeTags;
    context.enchantments = owner_->options().enchantments;
    context.world = [this] { return owner_->currentWorld(); };
    context.damaged = hooks_.damaged;
    context.healed = hooks_.healed;
    context.slotChanged = [this](const PlayerSession&, std::int32_t slot) { owner_->sendInventorySlot(slot); };
    context.disableBlocking = [this](PlayerSession& player, std::int32_t ticks) {
        owner_->itemUseTimers().stopBlocking();
        const world::LoadedWorld* world = owner_->currentWorld();
        player::startCooldownFor(player, Identifier::vanilla("shield"), world == nullptr ? 0 : world->gameTime(),
                                 ticks);
    };
    context.death.registry = owner_->options().playerRegistry;
    context.death.logger = owner_->logger();
    context.death.lang = owner_->consoleLang();
    context.death.died = hooks_.died;
    return context;
}

float CombatHandler::reduced(const PlayerSession& session, const float amount,
                             const std::optional<Identifier>& damageType) const {
    return player::damageAfterProtection(session, amount, damageType, damageContext());
}

void CombatHandler::hurt(const float amount, const std::string_view deathMessageId,
                         const std::optional<Identifier>& damageType,
                         const std::optional<entity::DamageSource>& source) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    player::hurtPlayer(*session, amount, deathMessageId, damageType, source, damageContext());
}

void CombatHandler::heal(const float amount) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    player::healPlayer(*session, amount, damageContext());
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
    // The death itself is the same on every edition, so it lives where a player who never had one
    // of these connections can reach it. What is left here is only which plugins to tell and where
    // to write the line.
    player::DeathContext context;
    context.registry = owner_->options().playerRegistry;
    context.logger = owner_->logger();
    context.lang = owner_->consoleLang();
    context.died = hooks_.died;
    player::killPlayer(*session, deathMessageId, killer, context);
}

} // namespace basalt::server::runtime
