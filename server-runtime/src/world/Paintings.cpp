#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/Paintings.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world::paintings {
namespace {

namespace reg = ::basalt::server::registry;

using Painting = registry::vanillaRegistryData::Painting;

/// parity: `Direction.getCounterClockWise`.
[[nodiscard]] reg::BlockFace leftOf(const reg::BlockFace facing) {
    switch (facing) {
    case reg::BlockFace::North:
        return reg::BlockFace::West;
    case reg::BlockFace::West:
        return reg::BlockFace::South;
    case reg::BlockFace::South:
        return reg::BlockFace::East;
    case reg::BlockFace::East:
        return reg::BlockFace::North;
    default:
        return facing;
    }
}

/// parity: `HangingEntity.survives`.
[[nodiscard]] bool fits(const WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                        const reg::BlockFace facing, const Painting& painting) {
    const reg::BlockFace left = leftOf(facing);
    // parity: `(size - 1) / -2`, which truncates towards zero, so an even side leans one way.
    const std::int32_t firstAcross = (painting.width - 1) / -2;
    const std::int32_t firstUp = (painting.height - 1) / -2;
    for (std::int32_t across = 0; across < painting.width; ++across) {
        for (std::int32_t up = 0; up < painting.height; ++up) {
            const std::int32_t stepAcross = across + firstAcross;
            const std::int32_t stepUp = up + firstUp;
            const std::int32_t coverX = x + (reg::stepX(left) * stepAcross);
            const std::int32_t coverY = y + stepUp;
            const std::int32_t coverZ = z + (reg::stepZ(left) * stepAcross);
            const std::optional<std::int32_t> behind =
                world.blockState(coverX - reg::stepX(facing), coverY, coverZ - reg::stepZ(facing));
            if (!behind.has_value()) {
                return false;
            }
            const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(*behind);
            const std::string_view path = block == nullptr ? std::string_view{} : block->path();
            // parity: `state.isSolid() || DiodeBlock.isDiode(state)`.
            if (!reg::blockShapes::isSolidState(*behind) && path != "repeater" && path != "comparator") {
                return false;
            }
            const std::optional<std::int32_t> covered = world.blockState(coverX, coverY, coverZ);
            if (!covered.has_value() || !reg::blockShapes::collisionBoxes(*covered).empty()) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

std::optional<Painting> fitting(const WorldState& world, const std::int32_t x, const std::int32_t y,
                                const std::int32_t z, const reg::BlockFace facing, random::RandomSource& random) {
    if (facing == reg::BlockFace::Up || facing == reg::BlockFace::Down) {
        return std::nullopt;
    }
    std::vector<const Painting*> candidates;
    std::int32_t largest = 0;
    for (const Painting& painting : registry::vanillaRegistryData::paintingVariants()) {
        if (!fits(world, x, y, z, facing, painting)) {
            continue;
        }
        largest = std::max(largest, painting.width * painting.height);
        candidates.push_back(&painting);
    }
    // parity: only the biggest by area are kept, and one of those is drawn at random.
    std::erase_if(candidates,
                  [largest](const Painting* painting) { return painting->width * painting->height < largest; });
    if (candidates.empty()) {
        return std::nullopt;
    }
    return *candidates[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(candidates.size())))];
}

bool survives(const WorldState& world, const entity::ServerEntity& painting) {
    if (!painting.sourceNbt.has_value() || !painting.sourceNbt->contains("Facing")) {
        return true;
    }
    const auto facing = static_cast<reg::BlockFace>(painting.sourceNbt->getByte("Facing"));
    std::string_view variant = painting.sourceNbt->getString("variant");
    if (variant.starts_with("minecraft:")) {
        variant.remove_prefix(std::string_view{"minecraft:"}.size());
    }
    const std::vector<Painting>& all = registry::vanillaRegistryData::paintingVariants();
    const auto art = std::ranges::find(all, variant, &Painting::id);
    if (art == all.end() || facing == reg::BlockFace::Up || facing == reg::BlockFace::Down) {
        return true;
    }
    // The inverse of `hangingAt`: the half blocks an even side shifts by, taken back off the middle.
    const reg::BlockFace left = leftOf(facing);
    const double across = art->width % 2 == 0 ? 0.5 : 0.0;
    const double up = art->height % 2 == 0 ? 0.5 : 0.0;
    const auto x = static_cast<std::int32_t>(std::floor(painting.x - (reg::stepX(left) * across)));
    const auto y = static_cast<std::int32_t>(std::floor(painting.y - up));
    const auto z = static_cast<std::int32_t>(std::floor(painting.z - (reg::stepZ(left) * across)));
    return fits(world, x, y, z, facing, *art);
}

std::array<double, 3> hangingAt(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                const reg::BlockFace facing, const std::int32_t width, const std::int32_t height) {
    // parity: `shiftToBlockWall`.
    constexpr double kAgainstWall = 0.46875;
    const reg::BlockFace left = leftOf(facing);
    const double across = width % 2 == 0 ? 0.5 : 0.0;
    const double up = height % 2 == 0 ? 0.5 : 0.0;
    return {x + 0.5 - (reg::stepX(facing) * kAgainstWall) + (reg::stepX(left) * across), y + 0.5 + up,
            z + 0.5 - (reg::stepZ(facing) * kAgainstWall) + (reg::stepZ(left) * across)};
}

} // namespace basalt::server::runtime::world::paintings
