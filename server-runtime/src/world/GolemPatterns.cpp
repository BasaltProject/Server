#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/GolemPatterns.hpp>

#include <algorithm>
#include <string_view>

namespace basalt::server::runtime::world::golemPatterns {

namespace {

using registry::BlockFace;

/// What one square of a pattern asks of the block on it.
enum class Wants : std::uint8_t { Pumpkin, SnowBlock, IronBlock, Air };

/// One of `trySpawnGolem`'s three patterns, all of them one aisle deep.
struct Shape {
    Golem kind;
    std::int32_t width;
    std::int32_t height;
    /// Row major, top row first, exactly as the aisle strings read.
    std::array<Wants, 9> cells;
    /// The square `spawnGolemInWorld` is handed, as right and down into the pattern.
    std::int32_t spawnRight;
    std::int32_t spawnDown;
};

// parity: `getOrCreateSnowGolemFull` and `getOrCreateIronGolemFull`, tried in that order because the
// first match is the one that gets built. `getOrCreateCopperGolemFull` comes after these two and is
// not here yet: it needs the copper chest that takes the body's place.
constexpr std::array<Shape, 2> kShapes{
    Shape{.kind = Golem::Snow,
          .width = 1,
          .height = 3,
          .cells = {Wants::Pumpkin, Wants::SnowBlock, Wants::SnowBlock},
          .spawnRight = 0,
          .spawnDown = 2},
    Shape{.kind = Golem::Iron,
          .width = 3,
          .height = 3,
          .cells = {Wants::Air, Wants::Pumpkin, Wants::Air, Wants::IronBlock, Wants::IronBlock, Wants::IronBlock,
                    Wants::Air, Wants::IronBlock, Wants::Air},
          .spawnRight = 1,
          .spawnDown = 2},
};

constexpr std::array<BlockFace, 6> kDirections{BlockFace::Down,  BlockFace::Up,   BlockFace::North,
                                               BlockFace::South, BlockFace::West, BlockFace::East};

struct Vec {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

[[nodiscard]] constexpr Vec vectorOf(const BlockFace face) noexcept {
    return Vec{registry::stepX(face), registry::stepY(face), registry::stepZ(face)};
}

[[nodiscard]] constexpr Vec cross(const Vec& a, const Vec& b) noexcept {
    return Vec{(a.y * b.z) - (a.z * b.y), (a.z * b.x) - (a.x * b.z), (a.x * b.y) - (a.y * b.x)};
}

/// parity: `BlockPattern.translateAndRotate`: right is forwards crossed with up, and `down` walks
/// against up, which is what makes the aisle read top row first.
[[nodiscard]] constexpr std::array<std::int32_t, 3>
translateAndRotate(const std::array<std::int32_t, 3>& origin, const BlockFace forwards, const BlockFace up,
                   const std::int32_t right, const std::int32_t down, const std::int32_t ahead) noexcept {
    const Vec forwardsVector = vectorOf(forwards);
    const Vec upVector = vectorOf(up);
    const Vec rightVector = cross(forwardsVector, upVector);
    return {origin[0] + (upVector.x * -down) + (rightVector.x * right) + (forwardsVector.x * ahead),
            origin[1] + (upVector.y * -down) + (rightVector.y * right) + (forwardsVector.y * ahead),
            origin[2] + (upVector.z * -down) + (rightVector.z * right) + (forwardsVector.z * ahead)};
}

[[nodiscard]] bool isPumpkin(const std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return false;
    }
    const std::string_view path = block->path();
    return path == "carved_pumpkin" || path == "jack_o_lantern";
}

[[nodiscard]] bool isNamed(const std::int32_t stateId, const std::string_view name) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path() == name;
}

[[nodiscard]] bool holds(const Wants wants, const std::int32_t stateId) {
    switch (wants) {
    case Wants::Pumpkin:
        return isPumpkin(stateId);
    case Wants::SnowBlock:
        return isNamed(stateId, "snow_block");
    case Wants::IronBlock:
        return isNamed(stateId, "iron_block");
    case Wants::Air:
        return registry::blocks::isAir(stateId);
    }
    return false;
}

/// parity: `BlockPattern.matches`, x outermost so a pattern gives up on the first wrong column.
[[nodiscard]] bool matches(const WorldState& world, const Shape& shape, const std::array<std::int32_t, 3>& origin,
                           const BlockFace forwards, const BlockFace up) {
    for (std::int32_t right = 0; right < shape.width; ++right) {
        for (std::int32_t down = 0; down < shape.height; ++down) {
            const std::array<std::int32_t, 3> at = translateAndRotate(origin, forwards, up, right, down, 0);
            const std::optional<std::int32_t> stateId = world.blockState(at[0], at[1], at[2]);
            if (!stateId.has_value()) {
                return false;
            }
            const Wants wants = shape.cells[static_cast<std::size_t>((down * shape.width) + right)];
            if (!holds(wants, *stateId)) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] Match matchOf(const Shape& shape, const std::array<std::int32_t, 3>& origin, const BlockFace forwards,
                            const BlockFace up) {
    Match match{.kind = shape.kind};
    // parity: `clearPatternBlocks` walks the whole rectangle, x outermost, and the empty corners of
    // the iron cross are in it: it writes air over squares that already were air.
    for (std::int32_t right = 0; right < shape.width; ++right) {
        for (std::int32_t down = 0; down < shape.height; ++down) {
            match.cells.push_back(translateAndRotate(origin, forwards, up, right, down, 0));
        }
    }
    match.spawn = translateAndRotate(origin, forwards, up, shape.spawnRight, shape.spawnDown, 0);
    return match;
}

} // namespace

std::optional<Match> find(const WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    for (const Shape& shape : kShapes) {
        // parity: `BlockPattern.find` walks a cube as wide as the pattern's longest side, and only
        // towards positive x, y and z: a pumpkin is never the bottom right of what it completes.
        const std::int32_t dist = std::max(std::max(shape.width, shape.height), 1);
        for (std::int32_t dz = 0; dz < dist; ++dz) {
            for (std::int32_t dy = 0; dy < dist; ++dy) {
                for (std::int32_t dx = 0; dx < dist; ++dx) {
                    const std::array<std::int32_t, 3> origin{x + dx, y + dy, z + dz};
                    for (const BlockFace forwards : kDirections) {
                        for (const BlockFace up : kDirections) {
                            if (up == forwards || up == registry::opposite(forwards)) {
                                continue;
                            }
                            if (matches(world, shape, origin, forwards, up)) {
                                return matchOf(shape, origin, forwards, up);
                            }
                        }
                    }
                }
            }
        }
    }
    return std::nullopt;
}

std::string_view entityTypeOf(const Golem kind) { return kind == Golem::Snow ? "snow_golem" : "iron_golem"; }

} // namespace basalt::server::runtime::world::golemPatterns
