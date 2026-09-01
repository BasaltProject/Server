#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/world/NaturalSpawnObstruction.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace basalt::server::runtime::world::naturalSpawnObstruction {
namespace {

using entity::Aabb;
using wire::common::Identifier;

template <typename Predicate>
[[nodiscard]] bool anyBlock(const Aabb& box, Predicate&& predicate) {
    const std::int32_t maxX = static_cast<std::int32_t>(std::ceil(box.maxX));
    const std::int32_t maxY = static_cast<std::int32_t>(std::ceil(box.maxY));
    const std::int32_t maxZ = static_cast<std::int32_t>(std::ceil(box.maxZ));
    for (std::int32_t x = static_cast<std::int32_t>(std::floor(box.minX)); x < maxX; ++x) {
        for (std::int32_t y = static_cast<std::int32_t>(std::floor(box.minY)); y < maxY; ++y) {
            for (std::int32_t z = static_cast<std::int32_t>(std::floor(box.minZ)); z < maxZ; ++z) {
                if (predicate(x, y, z)) {
                    return true;
                }
            }
        }
    }
    return false;
}

[[nodiscard]] bool blocksAllow(const Aabb& box, const BlockAt& blockAt) {
    return !anyBlock(box, [&box, &blockAt](std::int32_t x, std::int32_t y, std::int32_t z) {
        const std::optional<std::int32_t> state = blockAt(x, y, z);
        if (!state.has_value()) {
            // An unloaded block is not in the way: the spawner is asked about places the world has
            // not caught up with, and refusing there would be refusing at random.
            return false;
        }
        const std::span<const std::int32_t> shapes = registry::blockShapes::collisionBoxes(*state);
        for (std::size_t at = 0; at + 5 < shapes.size(); at += 6) {
            const Aabb shape{.minX = x + shapes[at] / 16.0,
                             .minY = y + shapes[at + 1] / 16.0,
                             .minZ = z + shapes[at + 2] / 16.0,
                             .maxX = x + shapes[at + 3] / 16.0,
                             .maxY = y + shapes[at + 4] / 16.0,
                             .maxZ = z + shapes[at + 5] / 16.0};
            if (shape.intersects(box)) {
                return true;
            }
        }
        return false;
    });
}

[[nodiscard]] bool containsAnyLiquid(const Aabb& box, const BlockAt& blockAt) {
    return anyBlock(box, [&blockAt](std::int32_t x, std::int32_t y, std::int32_t z) {
        const std::optional<std::int32_t> state = blockAt(x, y, z);
        return state.has_value() && registry::blockFluids::hasFluid(*state);
    });
}

} // namespace

Aabb spawnBox(const Identifier& type, double x, double y, double z) {
    return Aabb::of(x, y, z, registry::entityTypeShapes::spawnWidthOf(type),
                    registry::entityTypeShapes::spawnHeightOf(type));
}

Aabb entityBox(const Identifier& type, double x, double y, double z) {
    return Aabb::of(x, y, z, registry::entityTypeShapes::widthOf(type), registry::entityTypeShapes::heightOf(type));
}

bool allowsLiquids(const Identifier& type) {
    static constexpr std::array<std::string_view, 14> kWaterTypes{
        "axolotl",  "cod",        "dolphin", "drowned", "elder_guardian", "glow_squid",    "guardian",
        "nautilus", "pufferfish", "salmon",  "squid",   "tadpole",        "tropical_fish", "zombie_nautilus"};
    return type.path() == "strider" || std::ranges::find(kWaterTypes, type.path()) != kWaterTypes.end();
}

bool blocksAllow(const Identifier& type, double x, double y, double z, const BlockAt& blockAt) {
    if (!blocksAllow(spawnBox(type, x, y, z), blockAt)) {
        return false;
    }
    return allowsLiquids(type) || !containsAnyLiquid(entityBox(type, x, y, z), blockAt);
}

bool canBeCollidedWith(const Identifier& type) {
    const std::string& path = type.path();
    return path == "shulker" || path == "happy_ghast" || path.ends_with("_boat") || path.ends_with("_raft");
}

bool blocksBuilding(const Identifier& type) {
    const std::string& path = type.path();
    return entity::mobs::isMob(type) || path == "armor_stand" || path == "end_crystal" || path == "falling_block" ||
           path == "tnt" || path == "minecart" || path.ends_with("_minecart") || path.ends_with("_boat") ||
           path.ends_with("_raft");
}

bool entityBlocksSpawn(const entity::ServerEntity& other, const Aabb& spawnBox, const Aabb& entityBox) {
    if (other.removed) {
        return false;
    }
    const Aabb otherBox = Aabb::of(other.x, other.y, other.z, other.width(), other.height());
    return (canBeCollidedWith(other.type()) && otherBox.intersects(spawnBox)) ||
           (blocksBuilding(other.type()) && otherBox.intersects(entityBox));
}

} // namespace basalt::server::runtime::world::naturalSpawnObstruction
