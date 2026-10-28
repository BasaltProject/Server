#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/WeaponDamage.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::SoundSource;
namespace mth = wire::common::mth;

/// parity: `ThrownTrident.onHitEntity`'s base damage and `deflect(REVERSE.
constexpr float kTridentDamage = 8.0F;
constexpr double kBounceSideways = 0.02;
constexpr double kBounceUp = 0.2;
constexpr std::int32_t kPanicTicks = 40;

[[nodiscard]] storage::StoredItemStack stackOf(const ServerEntity& trident) {
    return storage::StoredItemStack{.id = "minecraft:trident", .count = 1, .components = trident.heldItemComponents};
}

/// parity: Channeling on the thrown stack, whose `post_attack` and `hit_block` summon the bolt.
[[nodiscard]] bool channels(const ServerEntity& trident) {
    return player::enchantmentLevel(stackOf(trident), Identifier::vanilla("channeling")) > 0;
}

} // namespace

std::int32_t CombatSystem::tridentLoyalty(ServerEntity& trident) {
    if (trident.tridentLoyalty < 0) {
        // parity: `getLoyaltyFromItem`, the return acceleration clamped to a byte.
        trident.tridentLoyalty = static_cast<std::int8_t>(
            std::clamp(world::weaponDamage::valueEffect(stackOf(trident), "minecraft:trident_return_acceleration", 0.0F,
                                                        options_.enchantmentDefinition),
                       0.0F, 127.0F));
    }
    return trident.tridentLoyalty;
}

void CombatSystem::tridentSound(const std::vector<PlayerSession*>& watchers, const ServerEntity& trident,
                                std::string_view sound, float volume) {
    if (trident.sourceNbt.has_value() && trident.sourceNbt->getBoolean("Silent")) {
        return;
    }
    broadcaster_.sendEffects(
        watchers, mth::floor(trident.x), mth::floor(trident.y), mth::floor(trident.z),
        server::registry::blocks::air().stateId,
        {world::blockEffect::Sound{
            .name = std::string{sound}, .source = SoundSource::Neutral, .volume = volume, .pitch = 1.0F}});
}

bool CombatSystem::tridentReturns(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                  const std::vector<ServerEntity*>& living, ServerEntity& trident) {
    // parity: five turns in the ground count as having struck.
    if (trident.inGroundTime > 4) {
        trident.tridentDealtDamage = true;
    }
    const std::int32_t loyalty = tridentLoyalty(trident);
    const float voidReturn = world.state().gameplay().items.tridentLoyaltyVoidReturnHeight;
    const bool fellOut = voidReturn < 0.0F && trident.y < static_cast<double>(voidReturn);
    if (loyalty <= 0 || (!trident.tridentDealtDamage && !trident.noPhysics && !fellOut) ||
        !trident.ownerId.has_value()) {
        return false;
    }
    const PlayerSession* player = nullptr;
    const ServerEntity* mob = nullptr;
    for (const PlayerSession* candidate : watchers) {
        if (candidate->entityId() == *trident.ownerId) {
            player = candidate;
        }
    }
    for (const ServerEntity* candidate : living) {
        if (candidate->entityId() == *trident.ownerId) {
            mob = candidate;
        }
    }
    if (player == nullptr && mob == nullptr) {
        return false;
    }
    // parity: `isAcceptibleReturnOwner`: alive, and no spectating player.
    const bool acceptable = player != nullptr
                                ? !player->dead && player->gamemode != wire::protocol::play::Gamemode::Spectator
                                : !mob->removed && !mob->dying && mob->health > 0.0;
    if (!acceptable) {
        if (trident.arrowPickup == ServerEntity::ArrowPickup::Allowed && options_.dropItem) {
            options_.dropItem(world, trident.x, trident.y + 0.1, trident.z,
                              server::world::loot::ItemStack{.item = Identifier::vanilla("trident"),
                                                             .count = 1,
                                                             .components = trident.heldItemComponents});
        }
        trident.removed = true;
        return true;
    }
    const double eyeX = player != nullptr ? player->x : mob->x;
    const double eyeY =
        player != nullptr ? player->y + world::fluids::eyeHeight(player->sneaking) : mob->y + (mob->height() * 0.85);
    const double eyeZ = player != nullptr ? player->z : mob->z;
    const double dx = eyeX - trident.x;
    const double dy = eyeY - trident.y;
    const double dz = eyeZ - trident.z;
    const double distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    // parity: a mob takes it back the moment it is within its width and a block.
    if (mob != nullptr && distance < mob->width() + 1.0) {
        trident.removed = true;
        return true;
    }
    trident.noPhysics = true;
    trident.y += dy * 0.015 * loyalty;
    const double pull = 0.05 * loyalty;
    const double scale = distance < 1.0E-4 ? 0.0 : pull / distance;
    trident.velocityX = (trident.velocityX * 0.95) + (dx * scale);
    trident.velocityY = (trident.velocityY * 0.95) + (dy * scale);
    trident.velocityZ = (trident.velocityZ * 0.95) + (dz * scale);
    if (!trident.tridentReturning) {
        tridentSound(watchers, trident, "item.trident.return", 10.0F);
        trident.tridentReturning = true;
    }
    return false;
}

CombatSystem::ArrowHit CombatSystem::tridentHitEntity(world::LoadedWorld& world,
                                                      const std::vector<PlayerSession*>& watchers,
                                                      const std::vector<ServerEntity*>& living, ServerEntity& trident,
                                                      double fromX, double fromY, double fromZ) {
    // parity: `findHitEntity` finds nothing once it has struck.
    if (trident.tridentDealtDamage) {
        return ArrowHit::Missed;
    }
    const std::optional<Target> target = targetAlong(watchers, living, trident, fromX, fromY, fromZ);
    if (!target.has_value()) {
        return ArrowHit::Missed;
    }
    if (!hitAllowed(world, trident, target->mob != nullptr ? target->mob->entityId() : target->player->entityId(),
                    static_cast<std::int32_t>(std::floor(trident.x)), static_cast<std::int32_t>(std::floor(trident.y)),
                    static_cast<std::int32_t>(std::floor(trident.z)))) {
        return ArrowHit::Missed;
    }
    if (trident.bounces) {
        bounceOff(trident, std::nullopt);
        return ArrowHit::Stopped;
    }
    // parity: `EnchantmentHelper.modifyDamage` on the eight, impaling's bonus among it.
    const Identifier victimType = target->mob != nullptr ? target->mob->type() : Identifier::vanilla("player");
    const world::realEntityTypeTags::TagMembers* entityTags = options_.entityTags;
    const world::weaponDamage::Victim victim{
        .type = victimType, .inTag = [entityTags, &victimType](const Identifier& tag) {
            return entityTags != nullptr && world::realEntityTypeTags::membersOf(*entityTags, tag).contains(victimType);
        }};
    const float damage =
        world::weaponDamage::of(stackOf(trident), kTridentDamage, options_.enchantmentDefinition, &victim);
    trident.tridentDealtDamage = true;
    const PlayerSession* owner = nullptr;
    for (const PlayerSession* candidate : watchers) {
        if (trident.ownerId == candidate->entityId()) {
            owner = candidate;
        }
    }
    const bool ownerLiving = owner != nullptr || (trident.ownerId.has_value() &&
                                                  std::ranges::any_of(living, [&trident](const ServerEntity* one) {
                                                      return one->entityId() == *trident.ownerId;
                                                  }));
    // parity: channeling's `post_attack`.
    const auto channel = [&](double x, double y, double z, SoundSource source) {
        if (!ownerLiving || !channels(trident) || !world.weather().isThundering() ||
            !world::naturalSpawner::canSeeSky(world, mth::floor(x), mth::floor(y), mth::floor(z))) {
            return;
        }
        if (options_.summonLightning) {
            options_.summonLightning(world, x, y, z,
                                     owner != nullptr ? std::optional{owner->entityId()} : std::nullopt);
        }
        broadcaster_.sendEffects(watchers, mth::floor(x), mth::floor(y), mth::floor(z),
                                 server::registry::blocks::air().stateId,
                                 {world::blockEffect::Sound{
                                     .name = "item.trident.thunder", .source = source, .volume = 5.0F, .pitch = 1.0F}});
    };
    if (target->mob != nullptr && world::endCrystals::isCrystal(target->mob->type())) {
        hitCrystal(world, watchers, *target->mob, false, false,
                   firedByPlayer(watchers, trident) ? trident.ownerId : std::nullopt);
    } else if (target->mob != nullptr &&
               (world::detectorRails::isMinecart(target->mob->type()) || entity::boats::isBoat(target->mob->type()))) {
        // parity: `ThrownTrident.onHitEntity` on a vehicle: the hit, then channeling on it.
        const double hitX = target->mob->x;
        const double hitY = target->mob->y;
        const double hitZ = target->mob->z;
        if (options_.hurtVehicle &&
            options_.hurtVehicle(world, *target->mob,
                                 VehicleSystem::Hit{.amount = damage,
                                                    .type = world::damageTypes::kTrident,
                                                    .causeId = trident.ownerId,
                                                    .directId = trident.entityId(),
                                                    .burningProjectile = trident.fireTicks > 0})) {
            channel(hitX, hitY, hitZ, SoundSource::Neutral);
        }
    } else if (target->mob != nullptr) {
        ServerEntity& mob = *target->mob;
        if (mob.dragon != nullptr) {
            mob.dragon->hitPart = target->dragonPart;
        }
        markWitherFriend(world, mob, trident.ownerId);
        entity::entityCombat::blame(mob, trident.ownerId, trident.entityId(), trident.type());
        const entity::entityCombat::Result result = entity::entityCombat::hurt(
            mob, damage, fromX, fromZ, hooks_, "trident", std::nullopt, firedByPlayer(watchers, trident));
        if (result.landed) {
            emitVibration(world, "entity_damage", mob.x, mob.y, mob.z, mob.uuid());
            rememberShooter(watchers, trident, mob);
            if (trident.ownerId.has_value()) {
                mob.hurtBy = *trident.ownerId;
            }
            mob.panicTicks = kPanicTicks;
            announceEntityHit(watchers, mob, world::damageTypes::kTrident, trident.entityId(), result.died);
            if (owner != nullptr && options_.advancementTriggers != nullptr) {
                world::triggerFacts::playerHurtEntity(*options_.advancementTriggers, world, *owner, mob, damage,
                                                      world::damageTypes::kTrident,
                                                      world::triggerFacts::ofEntity(world, trident));
            }
            channel(mob.x, mob.y, mob.z, SoundSource::Neutral);
        }
    } else if (target->player != nullptr) {
        if (target->player->connection().hurt) {
            target->player->connection().hurt(damage, "trident", world::damageTypes::kTrident,
                                              entity::DamageSource{.entityId = trident.entityId(),
                                                                   .x = fromX,
                                                                   .z = fromZ,
                                                                   .name = shooterName(watchers, living, trident),
                                                                   .ownerId = trident.ownerId});
        }
        if (!target->player->dead) {
            channel(target->player->x, target->player->y, target->player->z, SoundSource::Players);
        }
    }
    tridentSound(watchers, trident, "item.trident.hit", 1.0F);
    // parity: `ProjectileDeflection.REVERSE` at (0.02.
    trident.velocityX *= -kBounceSideways * 0.5;
    trident.velocityY *= -kBounceUp * 0.5;
    trident.velocityZ *= -kBounceSideways * 0.5;
    trident.yaw += 170.0F + (static_cast<float>(options_.nextInt ? options_.nextInt(1000) : 0) / 1000.0F * 20.0F);
    return ArrowHit::Stopped;
}

void CombatSystem::channelOnBlock(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                  const ServerEntity& trident, const entity::BlockHit& hit) {
    if (!channels(trident) || !world.weather().isThundering()) {
        return;
    }
    const std::optional<std::int32_t> stateId = world.state().blockState(hit.blockX, hit.blockY, hit.blockZ);
    const Identifier* block =
        stateId.has_value() ? server::registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    if (block == nullptr || options_.blockTags == nullptr ||
        !world::realBlockTags::contains(*options_.blockTags, "minecraft:lightning_rods", *block) ||
        !world::naturalSpawner::canSeeSky(world, hit.blockX, hit.blockY, hit.blockZ)) {
        return;
    }
    // parity: `BlockPos.clampLocationWithin`, the point pulled inside the rod's own square.
    constexpr double kInside = 1.0E-5;
    const double x = std::clamp(hit.x, hit.blockX + kInside, hit.blockX + 1.0 - kInside);
    const double y = std::clamp(hit.y, hit.blockY + kInside, hit.blockY + 1.0 - kInside);
    const double z = std::clamp(hit.z, hit.blockZ + kInside, hit.blockZ + 1.0 - kInside);
    std::optional<std::int32_t> cause;
    for (const PlayerSession* candidate : watchers) {
        if (trident.ownerId == candidate->entityId()) {
            cause = candidate->entityId();
        }
    }
    if (options_.summonLightning) {
        options_.summonLightning(world, x, y, z, cause);
    }
    broadcaster_.sendEffects(
        watchers, mth::floor(x), mth::floor(y), mth::floor(z), server::registry::blocks::air().stateId,
        {world::blockEffect::Sound{
            .name = "item.trident.thunder", .source = SoundSource::Neutral, .volume = 5.0F, .pitch = 1.0F}});
}

} // namespace basalt::server::runtime
