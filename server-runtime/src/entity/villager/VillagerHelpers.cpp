#include "BrainInternals.hpp"

#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::entity::villager {
namespace {

/// parity: the eye of a standing player, and of one sneaking.
constexpr double kPlayerEye = 1.62;
constexpr double kSneakingEye = 1.27;

/// parity: `Item.Properties.villagerFood`, the four foods and what each is worth.
[[nodiscard]] std::int32_t nutritionOf(const wire::common::Identifier& item) {
    const std::string& path = item.path();
    if (path == "bread") {
        return 4;
    }
    return path == "carrot" || path == "potato" || path == "beetroot" ? 1 : 0;
}

[[nodiscard]] std::int32_t maxStack(const wire::common::Identifier& item) {
    const std::int32_t size = server::registry::itemStacking::maxStackSize(item);
    return size > 0 ? size : 64;
}

} // namespace

namespace look {

ServerEntity* mob(Ctx& ctx, std::int32_t entityId) {
    ServerEntity* found = ctx.mob.world.entity(entityId);
    return found == nullptr || found->removed ? nullptr : found;
}

const ai::PlayerView* player(Ctx& ctx, std::int32_t entityId) { return ctx.mob.playerById(entityId); }

std::optional<std::array<double, 3>> feet(Ctx& ctx, std::int32_t entityId) {
    if (const ai::PlayerView* view = player(ctx, entityId); view != nullptr) {
        return std::array<double, 3>{view->x, view->y, view->z};
    }
    if (const ServerEntity* found = mob(ctx, entityId); found != nullptr) {
        return std::array<double, 3>{found->x, found->y, found->z};
    }
    return std::nullopt;
}

bool alive(Ctx& ctx, std::int32_t entityId) {
    if (player(ctx, entityId) != nullptr) {
        return true;
    }
    const ServerEntity* found = mob(ctx, entityId);
    return found != nullptr && !found->dying && found->health > 0.0;
}

double distanceSqr(Ctx& ctx, std::int32_t entityId) {
    const std::optional<std::array<double, 3>> at = feet(ctx, entityId);
    if (!at.has_value()) {
        return std::numeric_limits<double>::max();
    }
    return ctx.mob.distanceSquaredTo((*at)[0], (*at)[1], (*at)[2]);
}

std::optional<std::array<double, 3>> position(Ctx& ctx, const Tracker& tracker) {
    if (tracker.entityId == 0) {
        return tracker.spot;
    }
    std::optional<std::array<double, 3>> at = feet(ctx, tracker.entityId);
    if (!at.has_value() || !tracker.eyeHeight) {
        return at;
    }
    if (const ai::PlayerView* view = player(ctx, tracker.entityId); view != nullptr) {
        (*at)[1] += view->sneaking ? kSneakingEye : kPlayerEye;
    } else if (const ServerEntity* found = mob(ctx, tracker.entityId); found != nullptr) {
        (*at)[1] += mobBreathing::eyeHeightOf(*found);
    }
    return at;
}

std::optional<BlockPos> block(Ctx& ctx, const Tracker& tracker) {
    if (tracker.entityId == 0) {
        return tracker.block;
    }
    const std::optional<std::array<double, 3>> at = feet(ctx, tracker.entityId);
    if (!at.has_value()) {
        return std::nullopt;
    }
    return BlockPos{static_cast<std::int32_t>(std::floor((*at)[0])), static_cast<std::int32_t>(std::floor((*at)[1])),
                    static_cast<std::int32_t>(std::floor((*at)[2]))};
}

void walkAndLook(Ctx& ctx, const Tracker& target, float speed, std::int32_t closeEnough) {
    ctx.mem.lookTarget.set(target);
    ctx.mem.walkTarget.set(WalkTarget{.target = target, .speed = speed, .closeEnough = closeEnough});
}

void lockGaze(Ctx& ctx, ServerEntity& other, float speed, std::int32_t closeEnough) {
    VillagerState& theirs = villagerBrain::stateOf(other);
    // parity: `lookAtEachOther`, then the walk and look memories both ways.
    ctx.mem.lookTarget.set(Tracker::ofEntity(other.entityId(), true));
    theirs.memories.lookTarget.set(Tracker::ofEntity(ctx.self.entityId(), true));
    walkAndLook(ctx, Tracker::ofEntity(other.entityId(), true), speed, closeEnough);
    const Tracker back = Tracker::ofEntity(ctx.self.entityId(), true);
    theirs.memories.lookTarget.set(back);
    theirs.memories.walkTarget.set(WalkTarget{.target = back, .speed = speed, .closeEnough = closeEnough});
}

VillagerState* villagerState(Ctx& ctx, std::int32_t entityId) {
    ServerEntity* found = mob(ctx, entityId);
    if (found == nullptr || found->type().path() != "villager") {
        return nullptr;
    }
    return &villagerBrain::stateOf(*found);
}

bool matches(Ctx& ctx, std::int32_t entityId, Filter filter) {
    if (filter == Filter::kAny) {
        return true;
    }
    if (player(ctx, entityId) != nullptr) {
        return filter == Filter::kPlayer;
    }
    const ServerEntity* found = mob(ctx, entityId);
    if (found == nullptr) {
        return false;
    }
    const std::string_view path = found->type().path();
    switch (filter) {
    case Filter::kVillager:
        return path == "villager";
    case Filter::kCat:
        return path == "cat";
    case Filter::kPlayer:
        return false;
    default:
        break;
    }
    // parity: `SetEntityLookTarget.create(MobCategory, ...)`, the type's own spawn category.
    const std::string_view category = mobs::categoryOf(found->type());
    switch (filter) {
    case Filter::kCreature:
        return category == "creature";
    case Filter::kWaterCreature:
        return category == "water_creature";
    case Filter::kAxolotls:
        return category == "axolotls";
    case Filter::kUndergroundWaterCreature:
        return category == "underground_water_creature";
    case Filter::kWaterAmbient:
        return category == "water_ambient";
    case Filter::kMonster:
        return category == "monster";
    default:
        return false;
    }
}

std::optional<std::int32_t> closestVisible(Ctx& ctx, Filter filter, double maxDistanceSqr) {
    if (!ctx.mem.visibleLivingEntities.present()) {
        return std::nullopt;
    }
    for (const std::int32_t id : *ctx.mem.visibleLivingEntities.value) {
        if (matches(ctx, id, filter) && distanceSqr(ctx, id) <= maxDistanceSqr) {
            return id;
        }
    }
    return std::nullopt;
}

BlockPos blockPosition(const ServerEntity& mob) {
    return BlockPos{static_cast<std::int32_t>(std::floor(mob.x)), static_cast<std::int32_t>(std::floor(mob.y)),
                    static_cast<std::int32_t>(std::floor(mob.z))};
}

BlockPos pathStart(Ctx& ctx, const ServerEntity& mob) {
    const std::array<std::int32_t, 3> start = pathFinder::startOf(ctx.mob.world, mob.x, mob.y, mob.z, mob.onGround);
    return BlockPos{start[0], start[1], start[2]};
}

bool closerToCenterThan(const BlockPos& pos, const ServerEntity& mob, double distance) {
    const double dx = pos.x + 0.5 - mob.x;
    const double dy = pos.y + 0.5 - mob.y;
    const double dz = pos.z + 0.5 - mob.z;
    return (dx * dx) + (dy * dy) + (dz * dz) < distance * distance;
}

std::int32_t manhattan(const BlockPos& one, const BlockPos& other) {
    return std::abs(one.x - other.x) + std::abs(one.y - other.y) + std::abs(one.z - other.z);
}

bool isBaby(const ServerEntity& mob) { return mob.age < 0; }

} // namespace look

namespace villagerData {

bool hasJob(const ServerEntity& villager) {
    return villager.profession != "none" && villager.profession != "nitwit" && !villager.profession.empty();
}

bool canBreed(const ServerEntity& villager) {
    if (villager.villager == nullptr) {
        return false;
    }
    // parity: `BREEDING_FOOD_THRESHOLD`, twelve points eaten or carried.
    constexpr std::int32_t kThreshold = 12;
    return villager.villager->foodLevel + foodPoints(*villager.villager) >= kThreshold && !villager.sleeping &&
           villager.age == 0;
}

std::int32_t foodPoints(const VillagerState& state) {
    std::int32_t points = 0;
    for (const std::optional<server::world::loot::ItemStack>& slot : state.inventory) {
        if (slot.has_value()) {
            points += slot->count * nutritionOf(slot->item);
        }
    }
    return points;
}

void eatAndDigest(VillagerState& state) {
    constexpr std::int32_t kFull = 12;
    // parity: `eatUntilFull`, a slot at a time and an item at a time, stopping once full.
    for (std::optional<server::world::loot::ItemStack>& slot : state.inventory) {
        if (state.foodLevel >= kFull) {
            break;
        }
        if (!slot.has_value()) {
            continue;
        }
        const std::int32_t nutrition = nutritionOf(slot->item);
        if (nutrition == 0) {
            continue;
        }
        std::int32_t eaten = 0;
        while (eaten < slot->count && state.foodLevel < kFull) {
            state.foodLevel += nutrition;
            ++eaten;
        }
        slot->count -= eaten;
        if (slot->count <= 0) {
            slot.reset();
        }
    }
    state.foodLevel -= kFull;
}

std::int32_t countItem(const VillagerState& state, std::string_view item) {
    std::int32_t count = 0;
    for (const std::optional<server::world::loot::ItemStack>& slot : state.inventory) {
        if (slot.has_value() && slot->item.path() == item) {
            count += slot->count;
        }
    }
    return count;
}

void removeItem(VillagerState& state, std::string_view item, std::int32_t count) {
    for (std::optional<server::world::loot::ItemStack>& slot : state.inventory) {
        if (count <= 0) {
            return;
        }
        if (!slot.has_value() || slot->item.path() != item) {
            continue;
        }
        const std::int32_t taken = std::min(count, slot->count);
        slot->count -= taken;
        count -= taken;
        if (slot->count <= 0) {
            slot.reset();
        }
    }
}

std::int32_t addItem(VillagerState& state, const server::world::loot::ItemStack& stack) {
    std::int32_t left = stack.count;
    const std::int32_t max = maxStack(stack.item);
    // parity: `SimpleContainer.addItem`.
    for (std::optional<server::world::loot::ItemStack>& slot : state.inventory) {
        if (left <= 0) {
            return 0;
        }
        if (slot.has_value() && slot->item == stack.item && slot->components == stack.components && slot->count < max) {
            const std::int32_t moved = std::min(left, max - slot->count);
            slot->count += moved;
            left -= moved;
        }
    }
    for (std::optional<server::world::loot::ItemStack>& slot : state.inventory) {
        if (left <= 0) {
            return 0;
        }
        if (!slot.has_value()) {
            server::world::loot::ItemStack placed = stack;
            placed.count = left;
            slot = std::move(placed);
            left = 0;
        }
    }
    return left;
}

bool wantsToPickUp(const ServerEntity& villager, const VillagerState& state,
                   const server::world::loot::ItemStack& stack) {
    const std::string& path = stack.item.path();
    // parity: `ItemTags.VILLAGER_PICKS_UP`.
    const bool picks = path == "bread" || path == "wheat" || path == "beetroot" || path == "wheat_seeds" ||
                       path == "potato" || path == "carrot" || path == "beetroot_seeds" ||
                       path == "torchflower_seeds" || path == "pitcher_pod" || nutritionOf(stack.item) > 0 ||
                       (villager.profession == "farmer" && (path == "bone_meal"));
    if (!picks) {
        return false;
    }
    // parity: `SimpleContainer.canAddItem`.
    const std::int32_t max = maxStack(stack.item);
    return std::ranges::any_of(
        state.inventory, [&stack, max](const std::optional<server::world::loot::ItemStack>& slot) {
            return !slot.has_value() ||
                   (slot->item == stack.item && slot->components == stack.components && slot->count < max);
        });
}

} // namespace villagerData

} // namespace basalt::server::runtime::entity::villager
