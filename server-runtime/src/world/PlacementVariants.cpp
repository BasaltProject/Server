#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>

#include <algorithm>
#include <string>

namespace basalt::server::runtime::world {
namespace {

using registry::BlockFace;
using wire::common::Identifier;
using wire::protocol::play::Direction;

std::int32_t stepX(Direction direction) {
    if (direction == Direction::West) {
        return -1;
    }
    return direction == Direction::East ? 1 : 0;
}

std::int32_t stepY(Direction direction) {
    if (direction == Direction::Down) {
        return -1;
    }
    return direction == Direction::Up ? 1 : 0;
}

std::int32_t stepZ(Direction direction) {
    if (direction == Direction::North) {
        return -1;
    }
    return direction == Direction::South ? 1 : 0;
}

Direction opposite(Direction face) {
    switch (face) {
    case Direction::Down:
        return Direction::Up;
    case Direction::Up:
        return Direction::Down;
    case Direction::North:
        return Direction::South;
    case Direction::South:
        return Direction::North;
    case Direction::West:
        return Direction::East;
    case Direction::East:
        return Direction::West;
    }
    return Direction::Up;
}

bool isScaffolding(std::int32_t stateId) {
    const Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path() == "scaffolding";
}

/// The one name a wall pair can have, since every pair in 26.2 follows the same shape.
std::string wallCandidate(const std::string& path) {
    // A hanging sign puts the wall in front of the pair of words, not before the last one.
    static constexpr std::string_view kHangingSign = "_hanging_sign";
    if (path.size() > kHangingSign.size() && path.ends_with(kHangingSign)) {
        return path.substr(0, path.size() - kHangingSign.size()) + "_wall_hanging_sign";
    }
    const std::size_t cut = path.rfind('_');
    if (cut == std::string::npos) {
        return "wall_" + path;
    }
    return path.substr(0, cut) + "_wall_" + path.substr(cut + 1);
}

} // namespace

namespace wallVariants {

std::optional<Identifier> of(const Identifier& block) {
    const Identifier wall = Identifier::vanilla(wallCandidate(std::string{block.path()}));
    return registry::blocks::blockIdOf(wall).has_value() ? std::optional{wall} : std::nullopt;
}

} // namespace wallVariants

namespace scaffoldingPlacement {

std::optional<std::array<std::int32_t, 3>> target(const WorldState& world, std::int32_t minY, std::int32_t maxY,
                                                  std::int32_t clickedX, std::int32_t clickedY, std::int32_t clickedZ,
                                                  Direction face, float yaw, bool sneaking, bool inside) {
    const std::optional<std::int32_t> clicked = world.blockState(clickedX, clickedY, clickedZ);
    // Clicking anything but a scaffolding places the block the ordinary way, so nothing is walked.
    if (!clicked.has_value() || !isScaffolding(*clicked)) {
        return std::nullopt;
    }
    Direction direction = Direction::Up;
    if (sneaking) {
        direction = inside ? opposite(face) : face;
    } else if (face == Direction::Up) {
        direction = blockPlacement::playerDirection(yaw);
    }
    std::int32_t x = clickedX + stepX(direction);
    std::int32_t y = clickedY + stepY(direction);
    std::int32_t z = clickedZ + stepZ(direction);
    std::int32_t walked = 0;
    while (walked < kReach) {
        if (y < minY || y > maxY) {
            return std::nullopt;
        }
        const std::optional<std::int32_t> here = world.blockState(x, y, z);
        if (!here.has_value()) {
            return std::nullopt;
        }
        if (!isScaffolding(*here)) {
            return registry::blockShapes::isReplaceable(*here) ? std::optional{std::array<std::int32_t, 3>{x, y, z}}
                                                               : std::nullopt;
        }
        x += stepX(direction);
        y += stepY(direction);
        z += stepZ(direction);
        // Only a sideways walk runs out: a tower is climbed as far as it goes.
        if (direction != Direction::Up && direction != Direction::Down) {
            walked++;
        }
    }
    return std::nullopt;
}

} // namespace scaffoldingPlacement

namespace mossCarpetTopper {

std::optional<std::int32_t> above(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                  const std::function<bool()>& coin) {
    const std::optional<std::int32_t> above = world.blockState(x, y + 1, z);
    if (!above.has_value()) {
        return std::nullopt;
    }
    const Identifier* block = registry::blockStateDefinitions::blockOf(*above);
    const bool isCarpet = block != nullptr && block->path() == "pale_moss_carpet";
    if (isCarpet && registry::blockStateDefinitions::propertiesOf(*above)["bottom"] == "true") {
        return std::nullopt;
    }
    if (!isCarpet && !registry::blockShapes::isReplaceable(*above)) {
        return std::nullopt;
    }
    std::map<std::string, std::string> wanted{{"bottom", "false"}};
    bool standing = false;
    for (const auto& [face, name] : {std::pair{BlockFace::North, "north"}, std::pair{BlockFace::South, "south"},
                                     std::pair{BlockFace::West, "west"}, std::pair{BlockFace::East, "east"}}) {
        std::int32_t offsetX = 0;
        std::int32_t offsetZ = 0;
        if (face == BlockFace::West) {
            offsetX = -1;
        } else if (face == BlockFace::East) {
            offsetX = 1;
        } else if (face == BlockFace::North) {
            offsetZ = -1;
        } else {
            offsetZ = 1;
        }
        const std::int32_t beside = world.blockState(x + offsetX, y + 1, z + offsetZ).value_or(0);
        // The coin is tossed only where a side could stand, which is what makes the shape vary.
        const bool low = registry::blockShapes::isFaceSturdy(beside, registry::opposite(face)) && coin();
        standing = standing || low;
        wanted[name] = low ? "low" : "none";
    }
    if (!standing) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> topper =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla("pale_moss_carpet"), wanted);
    if (!topper.has_value() || *topper == *above) {
        return std::nullopt;
    }
    return *topper;
}

} // namespace mossCarpetTopper

namespace doubleBlocks {
namespace {

std::int32_t offsetX(const std::string& facing) {
    if (facing == "west") {
        return -1;
    }
    return facing == "east" ? 1 : 0;
}

std::int32_t offsetZ(const std::string& facing) {
    if (facing == "north") {
        return -1;
    }
    return facing == "south" ? 1 : 0;
}

} // namespace

std::optional<SecondHalf> secondHalf(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) {
    const Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto half = properties.find("half");
    if (half != properties.end() && half->second == "lower") {
        const std::optional<std::int32_t> upper =
            registry::blockStateDefinitions::stateIdOf(*block, {{"half", "upper"}}, stateId);
        if (!upper.has_value()) {
            return std::nullopt;
        }
        return SecondHalf{.x = x, .y = y + 1, .z = z, .stateId = *upper};
    }
    const auto part = properties.find("part");
    const auto facing = properties.find("facing");
    if (part != properties.end() && part->second == "foot" && facing != properties.end()) {
        const std::optional<std::int32_t> head =
            registry::blockStateDefinitions::stateIdOf(*block, {{"part", "head"}}, stateId);
        if (!head.has_value()) {
            return std::nullopt;
        }
        return SecondHalf{.x = x + offsetX(facing->second), .y = y, .z = z + offsetZ(facing->second), .stateId = *head};
    }
    return std::nullopt;
}

std::int32_t firstHalf(std::int32_t stateId) {
    const Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return stateId;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto half = properties.find("half");
    if (half != properties.end() && half->second != "lower") {
        // A stair's `half` is top or bottom, not upper or lower: only the pair blocks are turned.
        const registry::blockStateDefinitions::Definition* definition = registry::blockStateDefinitions::of(*block);
        if (definition != nullptr && std::ranges::contains(definition->valuesOf("half"), "upper")) {
            return registry::blockStateDefinitions::stateIdOf(*block, {{"half", "lower"}}, stateId).value_or(stateId);
        }
    }
    const auto part = properties.find("part");
    if (part != properties.end() && part->second != "foot") {
        return registry::blockStateDefinitions::stateIdOf(*block, {{"part", "foot"}}, stateId).value_or(stateId);
    }
    return stateId;
}

} // namespace doubleBlocks

} // namespace basalt::server::runtime::world
