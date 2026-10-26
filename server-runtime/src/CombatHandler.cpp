#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/config/PurpurRules.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerCooldowns.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerDeath.hpp>
#include <basalt/server/runtime/player/PlayerImpulse.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/Cushions.hpp>
#include <basalt/server/runtime/world/DamageProtection.hpp>
#include <basalt/server/runtime/world/FallLanding.hpp>
#include <basalt/server/runtime/world/FarmlandTrample.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/TurtleEggs.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
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

/// parity: `LivingEntity.getEffectiveGravity`.
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
    if (vehicle != nullptr && hooks_.dismountAllowed && !hooks_.dismountAllowed(session, *vehicle)) {
        return;
    }
    session.vehicleId.reset();
    if (vehicle == nullptr) {
        return;
    }
    const std::optional<std::array<double, 3>> spot = dismountSpot(session, *vehicle);
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
    // The server's copy of the position came from the cart, so it is told the same rather than left a tick behind.
    if (spot.has_value()) {
        owner_->teleportTo((*spot)[0], (*spot)[1], (*spot)[2], session.yaw, session.pitch);
        return;
    }
    owner_->teleportTo(session.x, session.y + kDismountLift, session.z, session.yaw, session.pitch);
}

void CombatHandler::trackFall(PlayerSession& session, double y, bool onGround,
                              const std::optional<std::int32_t>& feetState) {
    // parity: `resetFallDistance` from elsewhere, a mace's smash among them, lands here too.
    fallDistance = std::min(fallDistance, session.fallDistance);
    const bool climbing = feetState.has_value() && [&] {
        const std::optional<server::registry::BlockState> block = server::registry::blocks::byStateId(*feetState);
        return block.has_value() && climbableBlocks().contains(block->block.path());
    }();
    const bool cancelled =
        session.flying || session.gamemode == Gamemode::Creative || session.gamemode == Gamemode::Spectator ||
        (feetState.has_value() && (world::fluids::of(*feetState) != world::Submersion::None || climbing));
    if (cancelled) {
        resetFall(session);
        session.fallDistance = 0.0;
        // parity: `handleMovePlayer` lets go of the impulse in liquid.
        player::impulse::tryReset(session);
        return;
    }
    if (!onGround && y < session.y) {
        fallDistance += session.y - y;
    }
    // parity: `trackStartFallingPosition`, where the player is once any fall is counted.
    if (fallDistance > 0.0 && !fallStart.has_value()) {
        fallStart = std::array<double, 3>{session.x, y, session.z};
        // parity: `trackStartFallingPosition`.
        world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
        world::LoadedWorld* world = owner_->currentWorld();
        if (session.explosionImpact.has_value() && (*session.explosionImpact)[1] <= y && triggers != nullptr &&
            world != nullptr) {
            world::TriggerEvent event{.startPosition = session.explosionImpact};
            if (session.explosionCause.has_value()) {
                if (const entity::ServerEntity* cause = world->state().entity(*session.explosionCause);
                    cause != nullptr) {
                    event.entities.emplace("cause", world::triggerFacts::ofEntity(*world, *cause));
                }
            }
            triggers->fire("fall_after_explosion", session.name(), session.uuid().toString(), std::move(event));
        }
        session.explosionImpact.reset();
        session.explosionCause.reset();
    }
    if (onGround && !session.onGround) {
        // parity: `Entity.checkFallDamage`.
        if (fallDistance > 0.0) {
            if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
                world::vibrations::emit(
                    world->state(), "hit_ground", session.x, y, session.z, world->gameTime(),
                    world::triggerFacts::vibrationSourceOf(session, owner_->options().advancementTriggers));
                bounceOn(session, *world, y);
            }
        }
        applyFallDamage(session, y);
        resetFall(session);
    }
    if (onGround) {
        resetFall(session);
    }
    if (onGround || session.fallFlying) {
        player::impulse::tryReset(session);
    }
    session.fallDistance = fallDistance;
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
    // parity: `restituteMovementAfterCollisions`.
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
    if (world != nullptr && fallDistance > 0.0) {
        const wire::protocol::play::BlockPos soil{
            static_cast<std::int32_t>(std::floor(session.x)),
            static_cast<std::int32_t>(std::floor(landingY - world::fallLanding::kProbeDepth)),
            static_cast<std::int32_t>(std::floor(session.z))};
        const std::optional<std::int32_t> soilState = world->state().blockState(soil.x, soil.y, soil.z);
        const Identifier* soilBlock =
            soilState.has_value() ? server::registry::blockStateDefinitions::blockOf(*soilState) : nullptr;
        const auto& protectedAt = owner_->options().spawnProtected;
        if (soilBlock != nullptr && soilBlock->path() == "turtle_egg" &&
            config::PurpurRules::resolve(world->state().gameplay().blocks.turtleEggMobGriefingOverride, true) &&
            !(protectedAt && protectedAt(session, *world, soil.x, soil.z))) {
            // parity: `TurtleEggBlock.fallOn`, one in three.
            random::RandomSource eggRandom = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
            if (const std::optional<world::BlockChange> cracked =
                    world::turtleEggs::trample(world->state(), soil.x, soil.y, soil.z, 3, eggRandom);
                cracked.has_value()) {
                owner_->blockInteraction().broadcastBlockUpdate(soil, cracked->stateId);
                owner_->blockInteraction().notifyNeighbours(soil);
            }
        }
        if (soilBlock != nullptr && soilBlock->path() == "farmland" &&
            !(protectedAt && protectedAt(session, *world, soil.x, soil.z))) {
            const std::optional<storage::StoredItemStack> boots =
                session.inventory.at(player::PlayerInventory::kArmourStart);
            random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
            const world::farmlandTrample::Faller faller{
                .fallDistance = fallDistance,
                .width = 0.6F,
                .height = 1.8F,
                .isPlayer = true,
                .featherFalling =
                    boots.has_value() ? player::enchantmentLevel(*boots, Identifier::vanilla("feather_falling")) : 0};
            if (world::farmlandTrample::tramples(world->state().gameplay(), faller, random)) {
                const std::int32_t dirt = server::registry::blocks::named("dirt")->stateId;
                if (world->state().setBlockState(soil.x, soil.y, soil.z, dirt)) {
                    owner_->blockInteraction().broadcastBlockUpdate(soil, dirt);
                    owner_->blockInteraction().notifyNeighbours(soil);
                }
            }
        }
    }
    if (world != nullptr && !world->gameRules().get(world::gameRules::kFallDamage)) {
        return;
    }
    // parity: `Entity.checkFallDamage` hands the fall to the block under the feet.
    const std::optional<std::int32_t> under =
        world == nullptr ? std::nullopt
                         : world->state().blockState(
                               static_cast<std::int32_t>(std::floor(session.x)),
                               static_cast<std::int32_t>(std::floor(landingY - world::fallLanding::kProbeDepth)),
                               static_cast<std::int32_t>(std::floor(session.z)));
    const world::fallLanding::Landing landing =
        under.has_value() ? world::fallLanding::of(*under, session.sneaking) : world::fallLanding::Landing{};
    const double fallen = player::impulse::effectiveFallDistance(session, fallDistance, landingY);
    const std::int32_t damage = world::fallLanding::damage(fallen, landing, player::combat::kSafeFallDistance);
    if (damage <= 0) {
        return;
    }
    player::impulse::reset(session);
    session.stats.addFallen(fallDistance * landing.distanceScale + landing.extraDistance);
    if (landing.stalagmite) {
        owner_->hurt(static_cast<float>(damage), "stalagmite", world::damageTypes::kStalagmite);
        return;
    }
    owner_->hurt(static_cast<float>(damage), "fall", world::damageTypes::kFall);
}

player::DamageContext CombatHandler::damageContext() const {
    player::DamageContext context;
    context.registry = owner_->options().playerRegistry;
    context.triggers = owner_->options().advancementTriggers;
    context.damageTypeTags = owner_->options().damageTypeTags;
    context.enchantments = owner_->options().enchantments;
    context.teams =
        owner_->options().commands == nullptr ? nullptr : owner_->options().commands->environment().options().teams;
    context.world = [this] { return owner_->currentWorld(); };
    context.damaged = hooks_.damaged;
    context.healed = hooks_.healed;
    context.resurrectAllowed = hooks_.resurrectAllowed;
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
    context.death.lootOf = [this](PlayerSession& session) { return deathLootOf(session); };
    context.death.spread = [this](PlayerSession& session, const player::DeathLoot& loot) {
        spreadDeathLoot(session, loot);
    };
    context.death.labelOf = deathLabels();
    return context;
}

std::function<std::optional<player::DeathLabel>(std::int32_t)> CombatHandler::deathLabels() const {
    return [this](std::int32_t id) -> std::optional<player::DeathLabel> {
        const auto customName = [](const wire::nbt::NbtCompound& components) -> std::optional<Component> {
            const wire::nbt::NbtTag* name = components.find("minecraft:custom_name");
            return name == nullptr ? std::nullopt : std::optional{wire::text::componentNbt::deserialize(*name)};
        };
        if (player::PlayerRegistry* registry = owner_->options().playerRegistry; registry != nullptr) {
            for (const PlayerSession* other : registry->snapshot()) {
                if (other->entityId() != id) {
                    continue;
                }
                const std::optional<storage::StoredItemStack>& held = other->inventory.heldItem();
                return player::DeathLabel{.name = Component::text(other->name()),
                                          .heldItem = held.has_value() ? customName(held->components) : std::nullopt};
            }
        }
        const world::LoadedWorld* world = owner_->currentWorld();
        const entity::ServerEntity* mob = world == nullptr ? nullptr : world->state().entity(id);
        if (mob == nullptr) {
            return std::nullopt;
        }
        const std::optional<server::world::loot::ItemStack> held =
            entity::mobEquipment::get(*mob, entity::mobEquipment::Slot::MainHand);
        return player::DeathLabel{.name = entity::entityCombat::displayName(*mob),
                                  .heldItem = held.has_value() ? customName(held->components) : std::nullopt};
    };
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

void CombatHandler::heal(const float amount, const std::string_view cause) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    player::healPlayer(*session, amount, damageContext(), cause);
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
    // The death is shared across editions; this only picks which plugins to tell and where to write the line.
    player::DeathContext context;
    context.registry = owner_->options().playerRegistry;
    context.logger = owner_->logger();
    context.lang = owner_->consoleLang();
    context.died = hooks_.died;
    context.lootOf = [this](PlayerSession& session) { return deathLootOf(session); };
    context.spread = [this](PlayerSession& session, const player::DeathLoot& loot) { spreadDeathLoot(session, loot); };
    context.labelOf = deathLabels();
    player::killPlayer(*session, deathMessageId, killer, context);
}

} // namespace basalt::server::runtime
