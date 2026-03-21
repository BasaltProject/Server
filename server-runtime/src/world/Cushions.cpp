#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/Cushions.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace basalt::server::runtime::world::cushions {
namespace {

constexpr std::array<std::string_view, 16> kColours{"white", "orange", "magenta",    "light_blue", "yellow", "lime",
                                                    "pink",  "gray",   "light_gray", "cyan",       "purple", "blue",
                                                    "brown", "green",  "red",        "black"};

constexpr std::string_view kItemSuffix = "_cushion";

/// parity: `Cushion.hasAnchorBelow`'s sixty-fourth, and the eighth it looks further down.
constexpr double kAnchorDepth = 0.015625;
constexpr double kAnchorReach = 0.125;

/// parity: `AABB.nextDeflated`, one ulp in from every face.
[[nodiscard]] double up(double value) { return std::nextafter(value, std::numeric_limits<double>::infinity()); }

[[nodiscard]] double down(double value) { return std::nextafter(value, -std::numeric_limits<double>::infinity()); }

struct Box {
    double minX, minY, minZ, maxX, maxY, maxZ;

    [[nodiscard]] bool intersects(const Box& other) const {
        return minX < other.maxX && maxX > other.minX && minY < other.maxY && maxY > other.minY && minZ < other.maxZ &&
               maxZ > other.minZ;
    }
};

[[nodiscard]] Box boxAt(double x, double y, double z) {
    const double half = kWidth / 2.0;
    return Box{x - half, y, z - half, x + half, y + kHeight, z + half};
}

/// The blocks a box touches. parity: `BlockPos.betweenClosed` over the floored corners.
template <typename Visit>
bool forBlocksIn(const Box& box, Visit&& visit) {
    const auto lowX = static_cast<std::int32_t>(std::floor(box.minX));
    const auto lowY = static_cast<std::int32_t>(std::floor(box.minY));
    const auto lowZ = static_cast<std::int32_t>(std::floor(box.minZ));
    const auto highX = static_cast<std::int32_t>(std::floor(box.maxX));
    const auto highY = static_cast<std::int32_t>(std::floor(box.maxY));
    const auto highZ = static_cast<std::int32_t>(std::floor(box.maxZ));
    for (std::int32_t x = lowX; x <= highX; ++x) {
        for (std::int32_t y = lowY; y <= highY; ++y) {
            for (std::int32_t z = lowZ; z <= highZ; ++z) {
                if (visit(x, y, z)) {
                    return true;
                }
            }
        }
    }
    return false;
}

/// The bounding box of a block's shape, or nothing for an empty one.
[[nodiscard]] std::optional<Box> boundsOf(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::span<const std::int32_t> shape = registry::blockShapes::collisionBoxes(stateId);
    if (shape.size() < 6) {
        return std::nullopt;
    }
    const double units = registry::blockShapes::kUnits;
    Box bounds{std::numeric_limits<double>::max(),    std::numeric_limits<double>::max(),
               std::numeric_limits<double>::max(),    std::numeric_limits<double>::lowest(),
               std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()};
    for (std::size_t at = 0; at + 5 < shape.size(); at += 6) {
        bounds.minX = std::min(bounds.minX, x + (shape[at] / units));
        bounds.minY = std::min(bounds.minY, y + (shape[at + 1] / units));
        bounds.minZ = std::min(bounds.minZ, z + (shape[at + 2] / units));
        bounds.maxX = std::max(bounds.maxX, x + (shape[at + 3] / units));
        bounds.maxY = std::max(bounds.maxY, y + (shape[at + 4] / units));
        bounds.maxZ = std::max(bounds.maxZ, z + (shape[at + 5] / units));
    }
    return bounds;
}

/// parity: `Cushion.hasAnchorBelow`.
[[nodiscard]] bool hasAnchorBelow(const WorldState& world, const Box& box) {
    const Box anchor{box.minX, box.minY - kAnchorDepth, box.minZ, down(box.maxX), box.minY, down(box.maxZ)};
    Box searched = anchor;
    searched.minY -= kAnchorReach;
    return forBlocksIn(searched, [&](std::int32_t x, std::int32_t y, std::int32_t z) {
        const std::optional<std::int32_t> state = world.blockState(x, y, z);
        const std::optional<Box> bounds = state.has_value() ? boundsOf(*state, x, y, z) : std::nullopt;
        return bounds.has_value() && bounds->intersects(anchor);
    });
}

/// parity: `Cushion.isAnchorBuried`, the bottom sixty-fourth wholly under collision. Checked box by
/// box, which is every case a single block can make.
[[nodiscard]] bool isAnchorBuried(const WorldState& world, const Box& box) {
    const Box slice{up(box.minX),  up(box.minY), up(box.minZ), down(box.maxX), down(box.minY + kAnchorDepth),
                    down(box.maxZ)};
    return forBlocksIn(slice, [&](std::int32_t x, std::int32_t y, std::int32_t z) {
        const std::optional<std::int32_t> state = world.blockState(x, y, z);
        if (!state.has_value()) {
            return false;
        }
        const std::span<const std::int32_t> shape = registry::blockShapes::collisionBoxes(*state);
        const double units = registry::blockShapes::kUnits;
        for (std::size_t at = 0; at + 5 < shape.size(); at += 6) {
            if (x + (shape[at] / units) <= slice.minX && y + (shape[at + 1] / units) <= slice.minY &&
                z + (shape[at + 2] / units) <= slice.minZ && x + (shape[at + 3] / units) >= slice.maxX &&
                y + (shape[at + 4] / units) >= slice.maxY && z + (shape[at + 5] / units) >= slice.maxZ) {
                return true;
            }
        }
        return false;
    });
}

/// parity: `Cushion.isCoveredBySuffocatingBlocks`: every block the box is in suffocates.
[[nodiscard]] bool isCovered(const WorldState& world, const Box& box, const Tagged& tagged) {
    const Box inside{up(box.minX), up(box.minY), up(box.minZ), down(box.maxX), down(box.maxY), down(box.maxZ)};
    const bool anyFree = forBlocksIn(inside, [&](std::int32_t x, std::int32_t y, std::int32_t z) {
        const std::optional<std::int32_t> state = world.blockState(x, y, z);
        const wire::common::Identifier* block =
            state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
        const bool suffocates = block != nullptr && tagged && tagged("minecraft:causes_suffocation", *block) &&
                                registry::blockShapes::isFullCube(*state);
        return !suffocates;
    });
    return !anyFree;
}

} // namespace

const wire::common::Identifier& type() {
    static const wire::common::Identifier kType = wire::common::Identifier::vanilla("cushion");
    return kType;
}

bool isCushion(const wire::common::Identifier& entityType) { return entityType == type(); }

std::optional<std::string_view> colourOfItem(const wire::common::Identifier& item) {
    const std::string_view path = item.path();
    if (!path.ends_with(kItemSuffix)) {
        return std::nullopt;
    }
    const std::string_view colour = path.substr(0, path.size() - kItemSuffix.size());
    const auto found = std::ranges::find(kColours, colour);
    return found == kColours.end() ? std::nullopt : std::optional{*found};
}

std::optional<std::int32_t> dyeIdOf(std::string_view colour) {
    if (colour.starts_with("minecraft:")) {
        colour.remove_prefix(std::string_view{"minecraft:"}.size());
    }
    const auto found = std::ranges::find(kColours, colour);
    return found == kColours.end() ? std::nullopt : std::optional{static_cast<std::int32_t>(found - kColours.begin())};
}

std::string_view colourOf(const entity::ServerEntity& cushion) {
    const std::string_view saved = cushion.sourceNbt.has_value() ? cushion.sourceNbt->getString("color") : "";
    const std::optional<std::int32_t> id = dyeIdOf(saved);
    return kColours[static_cast<std::size_t>(id.value_or(0))];
}

wire::common::Identifier itemOf(const entity::ServerEntity& cushion) {
    return wire::common::Identifier::vanilla(std::string{colourOf(cushion)} + std::string{kItemSuffix});
}

bool canBePlacedAt(const WorldState& world, double x, double y, double z, const Tagged& tagged) {
    const Box box = boxAt(x, y, z);
    return survivesAt(world, x, y, z, tagged) && !isAnchorBuried(world, box);
}

bool survivesAt(const WorldState& world, double x, double y, double z, const Tagged& tagged) {
    const Box box = boxAt(x, y, z);
    return hasAnchorBelow(world, box) && !isCovered(world, box, tagged);
}

bool occupied(const WorldState& world, double x, double y, double z) {
    const Box box = boxAt(x, y, z);
    for (const entity::ServerEntity* other : world.entities()) {
        if (other == nullptr || other->removed || !isCushion(other->type())) {
            continue;
        }
        if (boxAt(other->x, other->y, other->z).intersects(box)) {
            return true;
        }
    }
    return false;
}

float snappedYaw(float yaw) {
    // parity: `Direction.fromYRot`, `floor(yRot / 90 + 0.5) & 3` into south, west, north, east.
    const auto quarter = static_cast<std::int32_t>(std::floor((static_cast<double>(yaw) / 90.0) + 0.5)) & 3;
    return static_cast<float>(quarter) * 90.0F;
}

std::optional<Clip> clipCollision(std::int32_t stateId, std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ,
                                  const std::array<double, 3>& from, const std::array<double, 3>& to) {
    const std::span<const std::int32_t> shape = registry::blockShapes::collisionBoxes(stateId);
    const std::array<double, 3> ray{to[0] - from[0], to[1] - from[1], to[2] - from[2]};
    if (shape.empty() || (ray[0] * ray[0]) + (ray[1] * ray[1]) + (ray[2] * ray[2]) < 1.0E-7) {
        return std::nullopt;
    }
    const double units = registry::blockShapes::kUnits;
    const auto boxOf = [&](std::size_t at) {
        return Box{blockX + (shape[at] / units),     blockY + (shape[at + 1] / units),
                   blockZ + (shape[at + 2] / units), blockX + (shape[at + 3] / units),
                   blockY + (shape[at + 4] / units), blockZ + (shape[at + 5] / units)};
    };
    // parity: a ray whose first thousandth is already inside answers there, facing back along it.
    const std::array<double, 3> probe{from[0] + (ray[0] * 0.001), from[1] + (ray[1] * 0.001),
                                      from[2] + (ray[2] * 0.001)};
    for (std::size_t at = 0; at + 5 < shape.size(); at += 6) {
        const Box box = boxOf(at);
        if (probe[0] >= box.minX && probe[0] < box.maxX && probe[1] >= box.minY && probe[1] < box.maxY &&
            probe[2] >= box.minZ && probe[2] < box.maxZ) {
            const double ax = std::abs(ray[0]);
            const double ay = std::abs(ray[1]);
            const double az = std::abs(ray[2]);
            const bool vertical = ay >= ax && ay >= az;
            return Clip{.up = vertical && ray[1] < 0.0, .y = probe[1]};
        }
    }
    // parity: `AABB.clip` over every box: the nearest entry wins, and only the Y face matters here.
    std::optional<Clip> best;
    double bestScale = 1.0;
    for (std::size_t at = 0; at + 5 < shape.size(); at += 6) {
        const Box box = boxOf(at);
        double enter = 0.0;
        double leave = 1.0;
        std::int32_t axisIn = -1;
        bool fits = true;
        const std::array<std::pair<double, double>, 3> spans{
            std::pair{box.minX, box.maxX}, std::pair{box.minY, box.maxY}, std::pair{box.minZ, box.maxZ}};
        for (std::int32_t axis = 0; axis < 3 && fits; ++axis) {
            const auto index = static_cast<std::size_t>(axis);
            if (ray[index] == 0.0) {
                fits = from[index] >= spans[index].first && from[index] <= spans[index].second;
                continue;
            }
            double near = (spans[index].first - from[index]) / ray[index];
            double far = (spans[index].second - from[index]) / ray[index];
            if (near > far) {
                std::swap(near, far);
            }
            if (near > enter) {
                enter = near;
                axisIn = axis;
            }
            leave = std::min(leave, far);
            fits = enter <= leave;
        }
        if (!fits || axisIn < 0 || enter >= bestScale) {
            continue;
        }
        bestScale = enter;
        best = Clip{.up = axisIn == 1 && ray[1] < 0.0, .y = from[1] + (ray[1] * enter)};
    }
    return best;
}

wire::nbt::NbtCompound freshTag(std::string_view colour, const std::array<std::int32_t, 3>& blockPos) {
    wire::nbt::NbtCompound tag;
    tag.putString("color", std::string{colour});
    tag.putIntArray("block_pos", {blockPos[0], blockPos[1], blockPos[2]});
    return tag;
}

} // namespace basalt::server::runtime::world::cushions
