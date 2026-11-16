#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RespawnAnchors.hpp>
#include <basalt/server/runtime/world/RespawnPoints.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world::respawnPoints {
namespace {

constexpr double kPlayerWidth = 0.6;
constexpr double kPlayerHeight = 1.8;

using Offset = std::array<std::int32_t, 3>;

struct Level {
    LoadedWorld& world;
    const realBlockTags::TagMembers* tags;

    [[nodiscard]] std::int32_t stateAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
        return world.state().blockState(x, y, z).value_or(registry::blocks::air().stateId);
    }

    [[nodiscard]] bool tagged(std::string_view tag, std::int32_t stateId) const {
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
        return tags != nullptr && block != nullptr && realBlockTags::contains(*tags, tag, *block);
    }
};

[[nodiscard]] std::string_view pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string_view{} : std::string_view{block->path()};
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

/// parity: `EntityType.isBlockDangerous` for a player.
[[nodiscard]] bool dangerous(const Level& level, std::int32_t stateId) {
    const std::string_view path = pathOf(stateId);
    if (path == "lava" || path == "magma_block" || path == "lava_cauldron" || level.tagged("minecraft:fire", stateId) ||
        ((path == "campfire" || path == "soul_campfire") && propertyOf(stateId, "lit") == "true")) {
        return true;
    }
    return path == "wither_rose" || path == "sweet_berry_bush" || path == "cactus" || path == "powder_snow";
}

/// parity: `DismountHelper.nonClimbableShape`, as the top of the shape.
[[nodiscard]] std::optional<double> nonClimbableTop(const Level& level, std::int32_t stateId) {
    if (level.tagged("minecraft:climbable", stateId) ||
        (pathOf(stateId).ends_with("_trapdoor") && propertyOf(stateId, "open") == "true")) {
        return std::nullopt;
    }
    if (registry::blockShapes::isEmptyShape(stateId)) {
        return std::nullopt;
    }
    const std::span<const std::int32_t> boxes = registry::blockShapes::collisionBoxes(stateId);
    std::int32_t top = std::numeric_limits<std::int32_t>::min();
    for (std::size_t at = 0; at + 5 < boxes.size(); at += 6) {
        top = std::max(top, boxes[at + 4]);
    }
    if (top == std::numeric_limits<std::int32_t>::min()) {
        return std::nullopt;
    }
    return static_cast<double>(top) / registry::blockShapes::kUnits;
}

/// parity: `DismountHelper.findSafeDismountLocation`.
[[nodiscard]] std::optional<Spot> safeDismount(const Level& level, std::int32_t x, std::int32_t y, std::int32_t z,
                                               bool checkDangerous) {
    const std::int32_t here = level.stateAt(x, y, z);
    if (checkDangerous && dangerous(level, here)) {
        return std::nullopt;
    }
    const std::int32_t below = level.stateAt(x, y - 1, z);
    const std::optional<double> own = nonClimbableTop(level, here);
    double floor = 0.0;
    if (own.has_value()) {
        floor = *own;
    } else if (const std::optional<double> beneath = nonClimbableTop(level, below); beneath.has_value()) {
        floor = *beneath - 1.0;
    } else {
        return std::nullopt;
    }
    if (floor >= 1.0) {
        return std::nullopt;
    }
    if (checkDangerous && floor <= 0.0 && dangerous(level, below)) {
        return std::nullopt;
    }
    const Spot spot{.x = x + 0.5, .y = y + floor, .z = z + 0.5};
    const entity::Aabb box = entity::Aabb::of(spot.x, spot.y, spot.z, kPlayerWidth, kPlayerHeight);
    const entity::entityCollision::BlockAt blockAt = [&level](std::int32_t bx, std::int32_t by, std::int32_t bz) {
        return std::optional{level.stateAt(bx, by, bz)};
    };
    if (entity::entityCollision::collides(blockAt, box)) {
        return std::nullopt;
    }
    if (level.tagged("minecraft:invalid_spawn_inside", here) ||
        level.tagged("minecraft:invalid_spawn_inside", level.stateAt(x, y + 1, z))) {
        return std::nullopt;
    }
    WorldBorder& border = level.world.border();
    const bool inside = border.contains(box.minX, box.minZ) && border.contains(box.maxX, box.maxZ);
    return inside ? std::optional{spot} : std::nullopt;
}

[[nodiscard]] std::optional<Spot> firstSafe(const Level& level, const Offset& origin,
                                            const std::vector<Offset>& offsets, bool checkDangerous) {
    for (const Offset& offset : offsets) {
        if (const std::optional<Spot> spot = safeDismount(level, origin[0] + offset[0], origin[1] + offset[1],
                                                          origin[2] + offset[2], checkDangerous)) {
            return spot;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<Spot> safeThenAny(const Level& level, const Offset& origin,
                                              const std::vector<Offset>& offsets) {
    if (std::optional<Spot> safe = firstSafe(level, origin, offsets, true)) {
        return safe;
    }
    return firstSafe(level, origin, offsets, false);
}

/// parity: `RespawnAnchorBlock.RESPAWN_OFFSETS`.
[[nodiscard]] const std::vector<Offset>& anchorOffsets() {
    static const std::vector<Offset> kOffsets = [] {
        const std::array<Offset, 8> horizontal{Offset{0, 0, -1},  Offset{-1, 0, 0}, Offset{0, 0, 1},  Offset{1, 0, 0},
                                               Offset{-1, 0, -1}, Offset{1, 0, -1}, Offset{-1, 0, 1}, Offset{1, 0, 1}};
        std::vector<Offset> all(horizontal.begin(), horizontal.end());
        for (const std::int32_t shift : {-1, 1}) {
            for (const Offset& flat : horizontal) {
                all.push_back(Offset{flat[0], shift, flat[2]});
            }
        }
        all.push_back(Offset{0, 1, 0});
        return all;
    }();
    return kOffsets;
}

[[nodiscard]] std::array<std::int32_t, 2> stepOf(std::string_view facing) {
    if (facing == "north") {
        return {0, -1};
    }
    if (facing == "south") {
        return {0, 1};
    }
    if (facing == "west") {
        return {-1, 0};
    }
    return {1, 0};
}

/// parity: `AbstractBedBlock.findStandUpPosition`.
[[nodiscard]] std::optional<Spot> bedStandUp(const Level& level, std::int32_t x, std::int32_t y, std::int32_t z,
                                             std::string_view facing, float yaw) {
    const std::array<std::int32_t, 2> f = stepOf(facing);
    // Clockwise from forward: the right-hand side, flipped when the player was facing it.
    std::array<std::int32_t, 2> s{-f[1], f[0]};
    const float radians = yaw * (std::numbers::pi_v<float> / 180.0F);
    if (static_cast<float>(s[0]) * -std::sin(radians) + static_cast<float>(s[1]) * std::cos(radians) > 0.0F) {
        s = {-s[0], -s[1]};
    }
    const auto at = [](std::int32_t dx, std::int32_t dz) { return Offset{dx, 0, dz}; };
    const std::vector<Offset> surround{
        at(s[0], s[1]),
        at(s[0] - f[0], s[1] - f[1]),
        at(s[0] - 2 * f[0], s[1] - 2 * f[1]),
        at(-2 * f[0], -2 * f[1]),
        at(-s[0] - 2 * f[0], -s[1] - 2 * f[1]),
        at(-s[0] - f[0], -s[1] - f[1]),
        at(-s[0], -s[1]),
        at(-s[0] + f[0], -s[1] + f[1]),
        at(f[0], f[1]),
        at(s[0] + f[0], s[1] + f[1]),
    };
    const std::vector<Offset> above{at(0, 0), at(-f[0], -f[1])};
    const Offset origin{x, y, z};
    if (!beds::isBed(level.stateAt(x, y - 1, z))) {
        std::vector<Offset> all = surround;
        all.insert(all.end(), above.begin(), above.end());
        return safeThenAny(level, origin, all);
    }
    const Offset lower{x, y - 1, z};
    for (const bool checkDangerous : {true, false}) {
        if (std::optional<Spot> spot = firstSafe(level, origin, surround, checkDangerous)) {
            return spot;
        }
        if (std::optional<Spot> spot = firstSafe(level, lower, surround, checkDangerous)) {
            return spot;
        }
        if (std::optional<Spot> spot = firstSafe(level, origin, above, checkDangerous)) {
            return spot;
        }
    }
    return std::nullopt;
}

/// parity: `RespawnPosAngle.of`, the yaw that looks at the block from the spot.
[[nodiscard]] Spot facing(Spot spot, std::int32_t x, std::int32_t z) {
    const double dx = x + 0.5 - spot.x;
    const double dz = z + 0.5 - spot.z;
    double yaw = std::atan2(dz, dx) * (180.0 / std::numbers::pi) - 90.0;
    yaw = std::fmod(yaw, 360.0);
    if (yaw >= 180.0) {
        yaw -= 360.0;
    } else if (yaw < -180.0) {
        yaw += 360.0;
    }
    spot.yaw = static_cast<float>(yaw);
    spot.pitch = 0.0F;
    return spot;
}

/// parity: `Block.isPossibleToRespawnInThis` and its sign, banner and pressure plate overrides.
[[nodiscard]] bool respawnsInside(std::int32_t stateId) {
    const std::string_view path = pathOf(stateId);
    if (path.ends_with("_sign") || path.ends_with("_banner") || path.ends_with("_pressure_plate")) {
        return true;
    }
    if (path == "water" || path == "lava" || path == "bubble_column") {
        return false;
    }
    return !registry::blockShapes::isSolidState(stateId);
}

void ensureAround(LoadedWorld& world, std::int32_t x, std::int32_t z) {
    for (std::int32_t chunkX = (x - 2) >> 4; chunkX <= (x + 2) >> 4; ++chunkX) {
        for (std::int32_t chunkZ = (z - 2) >> 4; chunkZ <= (z + 2) >> 4; ++chunkZ) {
            if (!world.state().isLoaded(chunkX, chunkZ)) {
                static_cast<void>(world.state().getOrGenerateChunk(chunkX, chunkZ));
            }
        }
    }
}

} // namespace

Outcome find(LoadedWorld& world, const Query& query, const realBlockTags::TagMembers* tags) {
    ensureAround(world, query.x, query.z);
    const Level level{world, tags};
    const std::int32_t state = level.stateAt(query.x, query.y, query.z);
    Outcome outcome;
    outcome.anchor = respawnAnchors::isAnchor(state);
    const std::int32_t charges = outcome.anchor ? respawnAnchors::chargesOf(state) : 0;
    const bool dark = beds::isNight(world.clock().totalTicks()) || world.weather().thundering;
    if (outcome.anchor && (query.forced || charges > 0) &&
        world.declaration().dimension == config::WorldDimension::Nether) {
        const std::optional<Spot> standUp = [&] {
            const std::vector<Offset>& offsets = anchorOffsets();
            return safeThenAny(level, Offset{query.x, query.y, query.z}, offsets);
        }();
        if (!query.forced && query.consumeCharge && standUp.has_value()) {
            outcome.spent = respawnAnchors::discharge(world.state(), query.x, query.y, query.z);
        }
        if (standUp.has_value()) {
            outcome.spot = facing(*standUp, query.x, query.z);
        }
        return outcome;
    }
    if (beds::isBed(state) &&
        beds::holds(beds::ruleFor(world.declaration().dimension, beds::isStrawBed(state)).canSetSpawn, dark)) {
        const std::optional<Spot> standUp =
            bedStandUp(level, query.x, query.y, query.z, beds::facingOf(state), query.yaw);
        if (standUp.has_value()) {
            outcome.spot = facing(*standUp, query.x, query.z);
        }
        return outcome;
    }
    if (!query.forced) {
        return outcome;
    }
    if (respawnsInside(state) && respawnsInside(level.stateAt(query.x, query.y + 1, query.z))) {
        outcome.spot = Spot{.x = query.x + 0.5, .y = query.y + 0.1, .z = query.z + 0.5, .yaw = query.yaw};
    }
    return outcome;
}

} // namespace basalt::server::runtime::world::respawnPoints
