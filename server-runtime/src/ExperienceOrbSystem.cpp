#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/ExperienceOrbSystem.hpp>
#include <basalt/server/runtime/entity/EntityPhysics.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;

/// parity: `mending`'s `repair_with_xp`, a multiply by two.
constexpr float kMendingFactor = 2.0F;

/// parity: `ExperienceOrb`'s own numbers.
constexpr std::int32_t kLifetime = 6000;
constexpr std::int32_t kMergeGroups = 40;
constexpr std::int32_t kScanPeriod = 20;
constexpr double kFollowDistance = 8.0;
constexpr double kGravity = 0.03;
constexpr double kAirDrag = static_cast<double>(0.98F);
constexpr double kBounce = 0.4;
constexpr std::int16_t kDefaultHealth = 5;
constexpr double kPlayerEyeHeight = 1.62;
constexpr std::int32_t kPickupDelay = 2;
/// parity: `ExperienceOrb.getExperienceValue`'s steps, largest first.
constexpr std::array<std::int32_t, 10> kSizes{2477, 1237, 617, 307, 149, 73, 37, 17, 7, 3};

[[nodiscard]] std::int32_t sizeFor(std::int32_t amount) {
    for (const std::int32_t size : kSizes) {
        if (amount >= size) {
            return size;
        }
    }
    return 1;
}

[[nodiscard]] wire::nbt::NbtCompound& tagOf(ServerEntity& orb) {
    if (!orb.sourceNbt.has_value()) {
        orb.sourceNbt.emplace();
    }
    return *orb.sourceNbt;
}

[[nodiscard]] std::int32_t countOf(const ServerEntity& orb) {
    return orb.sourceNbt.has_value() ? orb.sourceNbt->getInt("Count", 1) : 1;
}

[[nodiscard]] std::int32_t ageOf(const ServerEntity& orb) {
    return orb.sourceNbt.has_value() ? orb.sourceNbt->getShort("Age") : 0;
}

/// parity: `ExperienceOrb.canMerge`, which only lets one in forty ids join each other.
[[nodiscard]] bool canMerge(const ServerEntity& orb, std::int32_t id, std::int32_t value) {
    return !orb.removed && ExperienceOrbSystem::isOrb(orb) && (orb.entityId() - id) % kMergeGroups == 0 &&
           ExperienceOrbSystem::valueOf(orb) == value;
}

/// parity: `ExperienceOrb.repairPlayerItems`: a random damaged piece with mending, two points of
/// durability a point of experience, and whatever that leaves goes round again.
std::int32_t repairWithXp(PlayerSession& player, std::int32_t amount, random::RandomSource& random, bool& repaired) {
    // parity: `EquipmentSlot.VALUES`: main hand, off hand, then feet up to the head.
    const std::array<std::int32_t, 6> slots{player::PlayerInventory::kHotbarStart + player.inventory.heldSlot(),
                                            player::PlayerInventory::kOffhandSlot,
                                            player::PlayerInventory::kArmourStart + 3,
                                            player::PlayerInventory::kArmourStart + 2,
                                            player::PlayerInventory::kArmourStart + 1,
                                            player::PlayerInventory::kArmourStart};
    std::vector<std::int32_t> mendable;
    for (const std::int32_t slot : slots) {
        const std::optional<storage::StoredItemStack>& stack = player.inventory.at(slot);
        if (stack.has_value() && item::itemWear::isDamageable(*stack) && item::itemWear::damage(*stack) > 0 &&
            player::enchantmentLevel(*stack, Identifier::vanilla("mending")) > 0) {
            mendable.push_back(slot);
        }
    }
    if (mendable.empty()) {
        return amount;
    }
    const std::int32_t slot =
        mendable[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(mendable.size())))];
    const storage::StoredItemStack stack = *player.inventory.at(slot);
    const auto toRepair = std::max(0, static_cast<std::int32_t>(static_cast<float>(amount) * kMendingFactor));
    const std::int32_t repair = std::min(toRepair, item::itemWear::damage(stack));
    player.inventory.set(slot, item::itemWear::withDamage(stack, item::itemWear::damage(stack) - repair));
    repaired = true;
    if (repair > 0) {
        const std::int32_t remaining = amount - (repair * amount / toRepair);
        if (remaining > 0) {
            return repairWithXp(player, remaining, random, repaired);
        }
    }
    return 0;
}

} // namespace

ExperienceOrbSystem::ExperienceOrbSystem(player::PlayerRegistry& players, Options options)
    : players_(&players), options_(std::move(options)),
      random_(random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0)) {}

bool ExperienceOrbSystem::isOrb(const ServerEntity& entity) {
    return entity.type().namespaceName() == "minecraft" && entity.type().path() == "experience_orb";
}

std::int32_t ExperienceOrbSystem::valueOf(const ServerEntity& orb) {
    return orb.sourceNbt.has_value() ? orb.sourceNbt->getShort("Value") : 0;
}

void ExperienceOrbSystem::award(world::LoadedWorld& world, double x, double y, double z, std::int32_t amount) {
    const registry::EntityTypeEntry* entry = registry::entityTypes::of(Identifier::vanilla("experience_orb"));
    if (entry == nullptr) {
        return;
    }
    const std::scoped_lock guard{lock_};
    while (amount > 0) {
        const std::int32_t value = sizeFor(amount);
        amount -= value;
        // parity: `tryMergeToExisting`, any orb of that size in the block round the point.
        const std::int32_t group = random_.nextInt(kMergeGroups);
        bool merged = false;
        for (ServerEntity* other : world.state().entities()) {
            if (canMerge(*other, group, value) && std::abs(other->x - x) < 0.5 + (other->width() / 2.0) &&
                other->y < y + 0.5 && other->y + other->height() > y - 0.5 &&
                std::abs(other->z - z) < 0.5 + (other->width() / 2.0)) {
                tagOf(*other).putInt("Count", countOf(*other) + 1);
                tagOf(*other).putShort("Age", 0);
                merged = true;
                break;
            }
        }
        if (!merged) {
            spawnLocked(world, x, y, z, value);
        }
    }
}

void ExperienceOrbSystem::spawn(world::LoadedWorld& world, double x, double y, double z, std::int32_t value) {
    const std::scoped_lock guard{lock_};
    spawnLocked(world, x, y, z, value);
}

void ExperienceOrbSystem::spawnLocked(world::LoadedWorld& world, double x, double y, double z, std::int32_t value) {
    const registry::EntityTypeEntry* entry = registry::entityTypes::of(Identifier::vanilla("experience_orb"));
    if (entry == nullptr) {
        return;
    }
    auto orb = std::make_unique<ServerEntity>(players_->allocateEntityId(), wire::common::Uuid::random(), entry->id,
                                              entry->protocolId, x, y, z, random_.nextFloat() * 360.0F, 0.0F);
    // parity: a small random kick, up and to the side.
    orb->velocityX = ((random_.nextDouble() * 0.2) - 0.1) * 2.0;
    orb->velocityY = random_.nextDouble() * 0.2 * 2.0;
    orb->velocityZ = ((random_.nextDouble() * 0.2) - 0.1) * 2.0;
    wire::nbt::NbtCompound& tag = tagOf(*orb);
    tag.putShort("Health", kDefaultHealth);
    tag.putShort("Age", 0);
    tag.putShort("Value", static_cast<std::int16_t>(value));
    tag.putInt("Count", 1);
    static_cast<void>(world.state().spawnEntity(std::move(orb)));
}

void ExperienceOrbSystem::hurt(ServerEntity& orb, float damage) {
    const std::int32_t health = static_cast<std::int32_t>(
        (orb.sourceNbt.has_value() ? orb.sourceNbt->getShort("Health", kDefaultHealth) : kDefaultHealth) - damage);
    tagOf(orb).putShort("Health", static_cast<std::int16_t>(health));
    if (health <= 0) {
        orb.removed = true;
    }
}

void ExperienceOrbSystem::tickPlayers(const std::vector<PlayerSession*>& players) {
    const std::scoped_lock guard{lock_};
    for (const PlayerSession* player : players) {
        const auto found = takeDelays_.find(player->entityId());
        if (found != takeDelays_.end() && --found->second <= 0) {
            takeDelays_.erase(found);
        }
    }
}

void ExperienceOrbSystem::tick(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                               const std::vector<ServerEntity*>& living, ServerEntity& orb) {
    orb.tickCount++;
    const std::int32_t value = valueOf(orb);
    // parity: an orb inside a block does not fall.
    const std::optional<std::int32_t> inside = world.state().blockState(static_cast<std::int32_t>(std::floor(orb.x)),
                                                                        static_cast<std::int32_t>(std::floor(orb.y)),
                                                                        static_cast<std::int32_t>(std::floor(orb.z)));
    const bool colliding = inside.has_value() && !registry::blockShapes::isEmptyShape(*inside);
    if (orb.tickCount % kScanPeriod == 1) {
        // parity: `scanForMerges`, the orbs within half a block of this one's box.
        for (ServerEntity* other : living) {
            if (other != &orb && canMerge(*other, orb.entityId(), value) && std::abs(other->x - orb.x) < 1.0 &&
                std::abs(other->y - orb.y) < 1.0 && std::abs(other->z - orb.z) < 1.0) {
                tagOf(orb).putInt("Count", countOf(orb) + countOf(*other));
                tagOf(orb).putShort("Age", static_cast<std::int16_t>(std::min(ageOf(orb), ageOf(*other))));
                other->removed = true;
            }
        }
    }
    // parity: `followNearbyPlayer`, the nearest living, non-spectating player within eight blocks.
    PlayerSession* nearest = nullptr;
    double best = kFollowDistance * kFollowDistance;
    for (PlayerSession* player : watchers) {
        if (player->dead || player->gamemode == wire::protocol::play::Gamemode::Spectator) {
            continue;
        }
        const double distance = ((player->x - orb.x) * (player->x - orb.x)) +
                                ((player->y - orb.y) * (player->y - orb.y)) +
                                ((player->z - orb.z) * (player->z - orb.z));
        if (distance < best) {
            best = distance;
            nearest = player;
        }
    }
    if (nearest != nullptr) {
        const double dx = nearest->x - orb.x;
        const double dy = nearest->y + (kPlayerEyeHeight / 2.0) - orb.y;
        const double dz = nearest->z - orb.z;
        const double length = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
        if (length >= 1.0E-5) {
            const double power = 1.0 - (length / kFollowDistance);
            const double pull = power * power * 0.1;
            orb.velocityX += dx / length * pull;
            orb.velocityY += dy / length * pull;
            orb.velocityZ += dz / length * pull;
        }
    }
    const double fallSpeed = orb.velocityY;
    const std::int32_t below = world.state()
                                   .blockState(static_cast<std::int32_t>(std::floor(orb.x)),
                                               static_cast<std::int32_t>(std::floor(orb.y - 0.999999)),
                                               static_cast<std::int32_t>(std::floor(orb.z)))
                                   .value_or(0);
    const auto friction = static_cast<double>(registry::blockMovement::frictionOf(below));
    entity::entityPhysics::move(world.state(), orb, colliding ? 0.0 : kGravity, kAirDrag, friction, friction);
    // parity: an orb that lands hard bounces back up at forty percent.
    if (orb.onGround && fallSpeed < -kGravity) {
        orb.velocityY = -fallSpeed * kBounce;
    }
    const std::int32_t age = ageOf(orb) + 1;
    tagOf(orb).putShort("Age", static_cast<std::int16_t>(age));
    if (age >= kLifetime) {
        orb.removed = true;
        return;
    }
    // parity: `Player.touch` over the player's box grown by one, a half and one, and `playerTouch`.
    for (PlayerSession* player : watchers) {
        if (player->dead || player->gamemode == wire::protocol::play::Gamemode::Spectator) {
            continue;
        }
        const bool touching = std::abs(player->x - orb.x) < 0.3 + 1.0 + (orb.width() / 2.0) &&
                              orb.y < player->y + 1.8 + 0.5 && orb.y + orb.height() > player->y - 0.5 &&
                              std::abs(player->z - orb.z) < 0.3 + 1.0 + (orb.width() / 2.0);
        if (!touching) {
            continue;
        }
        {
            const std::scoped_lock guard{lock_};
            if (takeDelays_.contains(player->entityId())) {
                continue;
            }
            takeDelays_[player->entityId()] = kPickupDelay;
        }
        for (PlayerSession* watcher : watchers) {
            if (watcher->connection().sendPickupAnimation) {
                watcher->connection().sendPickupAnimation(orb.entityId(), player->entityId());
            }
        }
        bool repaired = false;
        std::int32_t remaining = 0;
        {
            const std::scoped_lock guard{lock_};
            remaining = repairWithXp(*player, value, random_, repaired);
        }
        if (repaired && player->connection().sendInventoryAll) {
            player->connection().sendInventoryAll();
        }
        if (remaining > 0) {
            player->experience.givePoints(remaining);
        }
        if (player->connection().syncExperience) {
            player->connection().syncExperience();
        }
        const std::int32_t left = countOf(orb) - 1;
        tagOf(orb).putInt("Count", left);
        if (left <= 0) {
            orb.removed = true;
            return;
        }
    }
}

} // namespace basalt::server::runtime
