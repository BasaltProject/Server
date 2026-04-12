#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/Leashes.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
namespace leashes = entity::leashes;
namespace mth = wire::common::mth;

/// parity: `Leashable.restoreLeashFromSave` gives up on a holder it cannot find after this long.
constexpr std::int32_t kRestoreGraceTicks = 100;
/// parity: the player's own box, which the lead is measured from.
constexpr double kPlayerWidth = 0.6;
constexpr double kPlayerHeight = 1.8;

[[nodiscard]] leashes::Body bodyOf(const ServerEntity& entity) {
    // parity: `getHolderMovement`: a mob with no AI is taken as standing still.
    const bool still = entity.noAi;
    return leashes::Body{.x = entity.x,
                         .y = entity.y,
                         .z = entity.z,
                         .yaw = entity.yaw,
                         .width = entity.width(),
                         .height = entity.height(),
                         .movement = still ? std::array<double, 3>{}
                                           : std::array{entity.velocityX, entity.velocityY, entity.velocityZ}};
}

[[nodiscard]] leashes::Body bodyOf(const PlayerSession& player) {
    return leashes::Body{.x = player.x,
                         .y = player.y,
                         .z = player.z,
                         .yaw = player.yaw,
                         .width = kPlayerWidth,
                         .height = kPlayerHeight,
                         .movement = player.knownMovement};
}

[[nodiscard]] bool isKnot(const ServerEntity& entity) { return entity.type().path() == "leash_knot"; }

} // namespace

PlayerSession* MobActionSystem::playerIn(world::LoadedWorld& world, std::int32_t entityId) {
    for (PlayerSession* player : watchersOf(world)) {
        if (player->entityId() == entityId) {
            return player;
        }
    }
    return nullptr;
}

void MobActionSystem::leashSound(world::LoadedWorld& world, std::string_view sound, double x, double y, double z) {
    random::RandomSource random = freshRandom();
    const std::int64_t seed = random.nextLong();
    for (const PlayerSession* watcher : watchersOf(world)) {
        player::playSoundFor(*watcher, sound, wire::protocol::play::SoundSource::Neutral, x, y, z, 1.0F, 1.0F, seed);
    }
}

void MobActionSystem::setLeashedTo(world::LoadedWorld& world, ServerEntity& leashed, std::int32_t holderId) {
    leashed.leashHolderId = holderId;
    leashed.leashKnotAt.reset();
    leashed.leashHolderUuid.reset();
    if (PlayerSession* player = playerIn(world, holderId); player != nullptr) {
        leashed.leashHolderUuid = player->uuid().toString();
    } else if (const ServerEntity* holder = world.state().entity(holderId); holder != nullptr) {
        if (isKnot(*holder)) {
            leashed.leashKnotAt = std::array{mth::floor(holder->x), mth::floor(holder->y), mth::floor(holder->z)};
        } else {
            leashed.leashHolderUuid = holder->uuid().toString();
        }
    }
    // parity: `setLeashedTo` takes it off whatever it was riding.
    if (leashed.vehicleId.has_value()) {
        if (ServerEntity* vehicle = world.state().entity(*leashed.vehicleId); vehicle != nullptr) {
            std::erase(vehicle->passengerIds, leashed.entityId());
        }
        leashed.vehicleId.reset();
    }
}

void MobActionSystem::dropLeash(world::LoadedWorld& world, ServerEntity& leashed, bool dropLead) {
    if (!leashed.leashHolderId.has_value()) {
        return;
    }
    const std::int32_t holderId = *std::exchange(leashed.leashHolderId, std::nullopt);
    leashed.leashHolderUuid.reset();
    leashed.leashKnotAt.reset();
    leashed.leashAngularMomentum = 0.0;
    // parity: `Mob.onLeashRemoved`, which lets go of the home the lead gave it.
    leashed.homePosition.reset();
    leashed.homeRadius = 0;
    if (dropLead) {
        random::RandomSource random = freshRandom();
        world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                   .uuid = wire::common::Uuid::random(),
                                                   .item = wire::common::Identifier::vanilla("lead"),
                                                   .count = 1,
                                                   .x = leashed.x,
                                                   .y = leashed.y,
                                                   .z = leashed.z,
                                                   .velocityX = (random.nextDouble() * 0.2) - 0.1,
                                                   .velocityY = 0.2,
                                                   .velocityZ = (random.nextDouble() * 0.2) - 0.1});
    }
    // parity: `LeashFenceKnotEntity.notifyLeasheeRemoved`, gone with its last lead.
    if (ServerEntity* holder = world.state().entity(holderId); holder != nullptr && isKnot(*holder)) {
        if (leashedTo(world, *holder, holder->x, holder->y, holder->z).empty()) {
            holder->removed = true;
        }
    }
}

std::vector<ServerEntity*> MobActionSystem::leashedTo(world::LoadedWorld& world, const ServerEntity& holder, double x,
                                                      double y, double z) {
    return leashedToId(world, holder.entityId(), x, y, z);
}

std::vector<ServerEntity*> MobActionSystem::leashedToId(world::LoadedWorld& world, std::int32_t holderId, double x,
                                                        double y, double z) {
    // parity: `Leashable.leashableInArea`, a cube of thirty-two around the centre.
    const double half = leashes::kAreaSize / 2.0;
    std::vector<ServerEntity*> found;
    for (ServerEntity* entity : world.state().entities()) {
        if (entity->removed || entity->leashHolderId != holderId || !leashes::isLeashable(*entity)) {
            continue;
        }
        const double halfWidth = entity->width() / 2.0;
        if (entity->x + halfWidth < x - half || entity->x - halfWidth > x + half ||
            entity->y + entity->height() < y - half || entity->y > y + half || entity->z + halfWidth < z - half ||
            entity->z - halfWidth > z + half) {
            continue;
        }
        found.push_back(entity);
    }
    return found;
}

bool MobActionSystem::canBeLeashed(const ServerEntity& entity) const {
    const bool monster = entity::mobs::categoryOf(entity.type()) == "monster";
    return !entity.dying && !entity.removed && leashes::canBeLeashed(entity, monster);
}

bool MobActionSystem::canHaveLeashAttachedTo(const ServerEntity& leashed, const leashes::Body& holder,
                                             std::int32_t holderId) const {
    return leashed.entityId() != holderId &&
           leashes::distance(bodyOf(leashed), holder) <= leashes::snapDistance(leashed) && canBeLeashed(leashed);
}

ServerEntity* MobActionSystem::knotAt(world::LoadedWorld& world, const std::array<std::int32_t, 3>& fence,
                                      bool create) {
    for (ServerEntity* entity : world.state().entities()) {
        if (!entity->removed && isKnot(*entity) && mth::floor(entity->x) == fence[0] &&
            mth::floor(entity->y) == fence[1] && mth::floor(entity->z) == fence[2]) {
            return entity;
        }
    }
    if (!create) {
        return nullptr;
    }
    const wire::common::Identifier type = wire::common::Identifier::vanilla("leash_knot");
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(type);
    if (entry == nullptr) {
        return nullptr;
    }
    // parity: `LeashFenceKnotEntity.recalculateBoundingBox`, in the middle of the fence post.
    auto created = std::make_unique<ServerEntity>(players_->allocateEntityId(), wire::common::Uuid::random(), type,
                                                  entry->protocolId, fence[0] + 0.5, fence[1] + leashes::kKnotOffsetY,
                                                  fence[2] + 0.5);
    created->noGravity = true;
    return &world.state().spawnEntity(std::move(created));
}

void MobActionSystem::tickLeash(world::LoadedWorld& world, ServerEntity& leashed) {
    // parity: `restoreLeashFromSave`: the saved holder found again, or the knot tied again, or the
    // lead dropped once it has waited long enough.
    if (!leashed.leashHolderId.has_value() &&
        (leashed.leashHolderUuid.has_value() || leashed.leashKnotAt.has_value())) {
        std::optional<std::int32_t> found;
        if (leashed.leashKnotAt.has_value()) {
            if (const ServerEntity* knot = knotAt(world, *leashed.leashKnotAt, true); knot != nullptr) {
                found = knot->entityId();
            }
        } else {
            for (PlayerSession* player : watchersOf(world)) {
                if (player->uuid().toString() == *leashed.leashHolderUuid) {
                    found = player->entityId();
                }
            }
            for (const ServerEntity* holder : world.state().entities()) {
                if (!found.has_value() && !holder->removed && holder->uuid().toString() == *leashed.leashHolderUuid) {
                    found = holder->entityId();
                }
            }
        }
        if (found.has_value()) {
            setLeashedTo(world, leashed, *found);
        } else if (leashed.tickCount > kRestoreGraceTicks) {
            leashed.leashHolderUuid.reset();
            leashed.leashKnotAt.reset();
            leashed.leashHolderId = -1;
            dropLeash(world, leashed, true);
        }
    }
    if (!leashed.leashHolderId.has_value()) {
        return;
    }
    const std::int32_t holderId = *leashed.leashHolderId;
    PlayerSession* player = playerIn(world, holderId);
    ServerEntity* holder = player == nullptr ? world.state().entity(holderId) : nullptr;
    // parity: `canInteractWithLevel`, alive and not a spectator, on both ends.
    const bool holderThere =
        (player != nullptr && !player->dead && player->gamemode != wire::protocol::play::Gamemode::Spectator) ||
        (holder != nullptr && !holder->removed && !holder->dying);
    if (!holderThere || leashed.dying || leashed.removed) {
        dropLeash(world, leashed, world.gameRules().get(world::gameRules::kEntityDrops));
        return;
    }
    const leashes::Body holderBody = player != nullptr ? bodyOf(*player) : bodyOf(*holder);
    const double distance = leashes::distance(bodyOf(leashed), holderBody);
    // parity: `PathfinderMob.whenLeashedTo`: home is where the holder stands, five blocks across.
    if (!entity::boats::isBoat(leashed.type())) {
        leashed.homePosition = std::array{mth::floor(holderBody.x), mth::floor(holderBody.y), mth::floor(holderBody.z)};
        leashed.homeRadius = static_cast<std::int32_t>(leashes::elasticDistance(leashed)) - 1;
    }
    if (distance > leashes::snapDistance(leashed)) {
        leashSound(world, "minecraft:entity.lead.break", holderBody.x, holderBody.y, holderBody.z);
        dropLeash(world, leashed, true);
        return;
    }
    const std::optional<leashes::Pull> pull =
        distance > leashes::elasticDistance(leashed) - holderBody.width - leashed.width()
            ? leashes::elastic(bodyOf(leashed), holderBody, leashes::elasticDistance(leashed))
            : std::nullopt;
    if (pull.has_value()) {
        leashed.leashAngularMomentum += pull->angularMomentum;
        leashed.velocityX += pull->push[0];
        leashed.velocityY += pull->push[1];
        leashed.velocityZ += pull->push[2];
    } else if (!entity::boats::isBoat(leashed.type()) && leashed.type().path() != "allay" &&
               leashed.type().path() != "happy_ghast" && leashed.panicTicks <= 0 && !leashed.noAi) {
        // parity: `PathfinderMob.closeRangeLeashBehaviour`: walk to two blocks short of the holder.
        const double dx = holderBody.x - leashed.x;
        const double dy = holderBody.y - leashed.y;
        const double dz = holderBody.z - leashed.z;
        const double length = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
        const double span = std::max(static_cast<double>(static_cast<float>(length)) - 2.0, 0.0);
        const double scale = length < 1.0e-4 ? 0.0 : span / length;
        const double speed = leashed.type().path() == "llama" || leashed.type().path() == "trader_llama" ? 2.0 : 1.0;
        static_cast<void>(entity::ai::navigation::moveTo(world.state(), leashed, leashed.x + (dx * scale),
                                                         leashed.y + (dy * scale), leashed.z + (dz * scale), speed));
    }
    // parity: the turn the rope gave it, worn down by what it stands on.
    leashed.yaw = static_cast<float>(static_cast<double>(leashed.yaw) - leashed.leashAngularMomentum);
    const std::int32_t under =
        world.state()
            .blockState(mth::floor(leashed.x), mth::floor(leashed.y - 0.500001), mth::floor(leashed.z))
            .value_or(0);
    leashed.leashAngularMomentum *= static_cast<double>(
        leashes::angularFriction(leashed.onGround, server::registry::blockMovement::frictionOf(under),
                                 entity::ai::inWater(world.state(), leashed)));
}

MobActionSystem::LeashUse MobActionSystem::useOnLeashable(world::LoadedWorld& world, PlayerSession& player,
                                                          ServerEntity& entity, bool holdingLead, bool holdingShears) {
    if (entity.removed || entity.dying || !leashes::isLeashable(entity)) {
        return LeashUse::Pass;
    }
    const leashes::Body here = bodyOf(entity);
    // parity: `Entity.interact`: a crouching player ties the mobs they hold to this one instead.
    if (player.sneaking && canBeLeashed(entity) && entity.age >= 0) {
        bool tied = false;
        for (ServerEntity* mob :
             leashedToId(world, player.entityId(), entity.x, entity.y + (entity.height() / 2.0), entity.z)) {
            if (canHaveLeashAttachedTo(*mob, here, entity.entityId())) {
                setLeashedTo(world, *mob, entity.entityId());
                tied = true;
            }
        }
        if (tied) {
            world::vibrations::emit(world.state(), "entity_action", entity.x, entity.y, entity.z, world.gameTime(),
                                    {.entity = player.uuid()});
            leashSound(world, "minecraft:entity.lead.tied", entity.x, entity.y, entity.z);
            return LeashUse::Done;
        }
    }
    // parity: `shearOffAllLeashConnections`: every lead on it, or tied to it, cut.
    if (holdingShears && cutLeashes(world, entity, &player)) {
        return LeashUse::DamageShears;
    }
    if (entity.leashHolderId == player.entityId()) {
        dropLeash(world, entity, player.gamemode != wire::protocol::play::Gamemode::Creative);
        world::vibrations::emit(world.state(), "entity_interact", entity.x, entity.y, entity.z, world.gameTime(),
                                {.entity = player.uuid()});
        leashSound(world, "minecraft:entity.lead.untied", entity.x, entity.y, entity.z);
        return LeashUse::Done;
    }
    const bool heldByPlayer = entity.leashHolderId.has_value() && playerIn(world, *entity.leashHolderId) != nullptr;
    if (!holdingLead || heldByPlayer || !canHaveLeashAttachedTo(entity, bodyOf(player), player.entityId())) {
        return LeashUse::Pass;
    }
    if (entity.leashHolderId.has_value()) {
        dropLeash(world, entity, true);
    }
    setLeashedTo(world, entity, player.entityId());
    leashSound(world, "minecraft:entity.lead.tied", entity.x, entity.y, entity.z);
    return LeashUse::ConsumeOne;
}

bool MobActionSystem::cutLeashes(world::LoadedWorld& world, ServerEntity& entity, const PlayerSession* player) {
    bool dropped = false;
    for (ServerEntity* mob : leashedTo(world, entity, entity.x, entity.y + (entity.height() / 2.0), entity.z)) {
        dropLeash(world, *mob, true);
        dropped = true;
    }
    if (entity.leashHolderId.has_value()) {
        dropLeash(world, entity, true);
        dropped = true;
    }
    if (!dropped) {
        return false;
    }
    world::vibrations::emit(world.state(), "shear", entity.x, entity.y, entity.z, world.gameTime(),
                            player == nullptr ? world::vibrations::Source{}
                                              : world::vibrations::Source{.entity = player->uuid()});
    random::RandomSource random = freshRandom();
    const std::int64_t seed = random.nextLong();
    for (const PlayerSession* watcher : watchersOf(world)) {
        player::playSoundFor(
            *watcher, "minecraft:item.shears.snip",
            player != nullptr ? wire::protocol::play::SoundSource::Players : wire::protocol::play::SoundSource::Neutral,
            std::floor(entity.x) + 0.5, std::floor(entity.y) + 0.5, std::floor(entity.z) + 0.5, 1.0F, 1.0F, seed);
    }
    return true;
}

MobActionSystem::LeashUse MobActionSystem::useOnKnot(world::LoadedWorld& world, PlayerSession& player,
                                                     ServerEntity& knot, bool holdingShears) {
    if (holdingShears && cutLeashes(world, knot, &player)) {
        return LeashUse::DamageShears;
    }
    const leashes::Body knotBody = bodyOf(knot);
    bool attached = false;
    for (ServerEntity* mob :
         leashedToId(world, player.entityId(), player.x, player.y + (kPlayerHeight / 2.0), player.z)) {
        if (canHaveLeashAttachedTo(*mob, knotBody, knot.entityId())) {
            setLeashedTo(world, *mob, knot.entityId());
            attached = true;
        }
    }
    bool taken = false;
    if (!attached && !player.sneaking) {
        for (ServerEntity* mob : leashedTo(world, knot, knot.x, knot.y + (knot.height() / 2.0), knot.z)) {
            if (canHaveLeashAttachedTo(*mob, bodyOf(player), player.entityId())) {
                setLeashedTo(world, *mob, player.entityId());
                taken = true;
            }
        }
    }
    if (!attached && !taken) {
        return LeashUse::Pass;
    }
    world::vibrations::emit(world.state(), "block_attach", knot.x, knot.y, knot.z, world.gameTime(),
                            {.entity = player.uuid()});
    leashSound(world, "minecraft:entity.lead.tied", knot.x, knot.y, knot.z);
    // parity: the knot a mob was taken off goes with its last lead.
    if (taken && leashedTo(world, knot, knot.x, knot.y, knot.z).empty()) {
        knot.removed = true;
    }
    return LeashUse::Done;
}

bool MobActionSystem::bindPlayerMobs(world::LoadedWorld& world, PlayerSession& player,
                                     const std::array<std::int32_t, 3>& fence) {
    // parity: `LeadItem.bindPlayerMobs`: every mob the player holds within reach, onto one knot.
    const std::vector<ServerEntity*> held =
        leashedToId(world, player.entityId(), fence[0] + 0.5, fence[1] + 0.5, fence[2] + 0.5);
    if (held.empty()) {
        return false;
    }
    ServerEntity* existing = knotAt(world, fence, false);
    ServerEntity* knot = existing != nullptr ? existing : knotAt(world, fence, true);
    if (knot == nullptr) {
        return false;
    }
    bool tied = false;
    const leashes::Body knotBody = bodyOf(*knot);
    for (ServerEntity* mob : held) {
        if (canHaveLeashAttachedTo(*mob, knotBody, knot->entityId())) {
            setLeashedTo(world, *mob, knot->entityId());
            tied = true;
        }
    }
    if (!tied) {
        if (existing == nullptr) {
            knot->removed = true;
        }
        return false;
    }
    leashSound(world, "minecraft:entity.lead.tied", knot->x, knot->y, knot->z);
    world::vibrations::emit(world.state(), "block_attach", fence[0] + 0.5, fence[1] + 0.5, fence[2] + 0.5,
                            world.gameTime(), {.entity = player.uuid()});
    return true;
}

bool MobActionSystem::dropAllLeashes(world::LoadedWorld& world, PlayerSession& player) {
    // parity: `Entity.dropAllLeashConnections(null)` for a player: every mob it holds let go.
    const std::vector<ServerEntity*> held =
        leashedToId(world, player.entityId(), player.x, player.y + (kPlayerHeight / 2.0), player.z);
    for (ServerEntity* mob : held) {
        dropLeash(world, *mob, true);
    }
    if (held.empty()) {
        return false;
    }
    world::vibrations::emit(world.state(), "shear", player.x, player.y, player.z, world.gameTime(), {});
    return true;
}

} // namespace basalt::server::runtime
