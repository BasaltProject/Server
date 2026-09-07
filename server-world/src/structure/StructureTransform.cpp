#include <basalt/server/world/structure/StructureTransform.hpp>

#include <array>
#include <cstdint>

namespace basalt::server::world::structure {
namespace {

struct Offset {
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;
    std::string_view name;
};

constexpr std::array<Offset, 6> kOffsets{Offset{0, -1, 0, "down"}, Offset{0, 1, 0, "up"},    Offset{0, 0, -1, "north"},
                                         Offset{0, 0, 1, "south"}, Offset{-1, 0, 0, "west"}, Offset{1, 0, 0, "east"}};

[[nodiscard]] const Offset& offsetOf(StructureDirection direction) {
    return kOffsets[static_cast<std::size_t>(direction)];
}

[[nodiscard]] bool equalsIgnoringCase(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        const char a = left[index] >= 'A' && left[index] <= 'Z' ? static_cast<char>(left[index] + 32) : left[index];
        if (a != right[index]) {
            return false;
        }
    }
    return true;
}

} // namespace

std::string_view serializedName(const StructureRotation rotation) {
    switch (rotation) {
    case StructureRotation::Clockwise90:
        return "clockwise_90";
    case StructureRotation::Clockwise180:
        return "clockwise_180";
    case StructureRotation::Counterclockwise90:
        return "counterclockwise_90";
    case StructureRotation::None:
    default:
        return "none";
    }
}

std::string_view serializedName(const StructureMirror mirror) {
    switch (mirror) {
    case StructureMirror::LeftRight:
        return "left_right";
    case StructureMirror::FrontBack:
        return "front_back";
    case StructureMirror::None:
    default:
        return "none";
    }
}

std::int32_t dx(const StructureDirection direction) { return offsetOf(direction).x; }

std::int32_t dy(const StructureDirection direction) { return offsetOf(direction).y; }

std::int32_t dz(const StructureDirection direction) { return offsetOf(direction).z; }

StructurePos offsetBy(const StructureDirection direction, const StructurePos pos) {
    const Offset& offset = offsetOf(direction);
    return StructurePos{.x = pos.x + offset.x, .y = pos.y + offset.y, .z = pos.z + offset.z};
}

bool isHorizontal(const StructureDirection direction) {
    return direction == StructureDirection::North || direction == StructureDirection::South ||
           direction == StructureDirection::West || direction == StructureDirection::East;
}

StructureDirection opposite(const StructureDirection direction) {
    switch (direction) {
    case StructureDirection::Down:
        return StructureDirection::Up;
    case StructureDirection::Up:
        return StructureDirection::Down;
    case StructureDirection::North:
        return StructureDirection::South;
    case StructureDirection::South:
        return StructureDirection::North;
    case StructureDirection::West:
        return StructureDirection::East;
    case StructureDirection::East:
    default:
        return StructureDirection::West;
    }
}

StructureDirection mirrored(const StructureDirection direction, const StructureMirror mirror) {
    if (mirror == StructureMirror::LeftRight) {
        if (direction == StructureDirection::North) {
            return StructureDirection::South;
        }
        if (direction == StructureDirection::South) {
            return StructureDirection::North;
        }
        return direction;
    }
    if (mirror == StructureMirror::FrontBack) {
        if (direction == StructureDirection::East) {
            return StructureDirection::West;
        }
        if (direction == StructureDirection::West) {
            return StructureDirection::East;
        }
        return direction;
    }
    return direction;
}

StructureDirection rotated(const StructureDirection direction, const StructureRotation rotation) {
    if (!isHorizontal(direction)) {
        return direction;
    }
    constexpr std::array<StructureDirection, 4> kCycle{StructureDirection::North, StructureDirection::East,
                                                       StructureDirection::South, StructureDirection::West};
    std::int32_t steps = 0;
    switch (rotation) {
    case StructureRotation::Clockwise90:
        steps = 1;
        break;
    case StructureRotation::Clockwise180:
        steps = 2;
        break;
    case StructureRotation::Counterclockwise90:
        steps = 3;
        break;
    case StructureRotation::None:
    default:
        steps = 0;
        break;
    }
    for (std::size_t index = 0; index < kCycle.size(); ++index) {
        if (kCycle[index] == direction) {
            return kCycle[(index + static_cast<std::size_t>(steps)) % kCycle.size()];
        }
    }
    return direction;
}

std::string_view serializedName(const StructureDirection direction) { return offsetOf(direction).name; }

std::optional<StructureDirection> directionFromName(const std::string_view name) {
    for (std::size_t index = 0; index < kOffsets.size(); ++index) {
        if (equalsIgnoringCase(name, kOffsets[index].name)) {
            return static_cast<StructureDirection>(index);
        }
    }
    return std::nullopt;
}

StructurePos transform(const StructurePos pos, const StructureMirror mirror, const StructureRotation rotation,
                       const StructurePos pivot) {
    std::int32_t x = pos.x;
    std::int32_t z = pos.z;
    bool wasMirrored = true;
    switch (mirror) {
    case StructureMirror::LeftRight:
        z = -z;
        break;
    case StructureMirror::FrontBack:
        x = -x;
        break;
    case StructureMirror::None:
    default:
        wasMirrored = false;
        break;
    }
    const std::int32_t pivotX = pivot.x;
    const std::int32_t pivotZ = pivot.z;
    switch (rotation) {
    case StructureRotation::Counterclockwise90:
        return StructurePos{.x = pivotX - pivotZ + z, .y = pos.y, .z = pivotX + pivotZ - x};
    case StructureRotation::Clockwise90:
        return StructurePos{.x = pivotX + pivotZ - z, .y = pos.y, .z = pivotZ - pivotX + x};
    case StructureRotation::Clockwise180:
        return StructurePos{.x = pivotX + pivotX - x, .y = pos.y, .z = pivotZ + pivotZ - z};
    case StructureRotation::None:
    default:
        return wasMirrored ? StructurePos{.x = x, .y = pos.y, .z = z} : pos;
    }
}

} // namespace basalt::server::world::structure

namespace basalt::server::world::structure {

StructureRotation rotated(StructureRotation rotation, StructureRotation by) {
    return static_cast<StructureRotation>((static_cast<std::int32_t>(rotation) + static_cast<std::int32_t>(by)) % 4);
}

StructurePos relative(StructurePos pos, StructureDirection direction, std::int32_t distance) {
    return StructurePos{pos.x + dx(direction) * distance, pos.y + dy(direction) * distance,
                        pos.z + dz(direction) * distance};
}

StructurePos rotate(StructurePos pos, StructureRotation rotation) {
    switch (rotation) {
    case StructureRotation::Clockwise90:
        return StructurePos{-pos.z, pos.y, pos.x};
    case StructureRotation::Clockwise180:
        return StructurePos{-pos.x, pos.y, -pos.z};
    case StructureRotation::Counterclockwise90:
        return StructurePos{pos.z, pos.y, -pos.x};
    case StructureRotation::None:
    default:
        return pos;
    }
}

StructurePos zeroPositionWithTransform(StructurePos zeroPos, StructureMirror mirror, StructureRotation rotation,
                                       std::int32_t width, std::int32_t depth) {
    const std::int32_t sizeX = width - 1;
    const std::int32_t sizeZ = depth - 1;
    const std::int32_t mirrorDeltaX = mirror == StructureMirror::FrontBack ? sizeX : 0;
    const std::int32_t mirrorDeltaZ = mirror == StructureMirror::LeftRight ? sizeZ : 0;
    switch (rotation) {
    case StructureRotation::Counterclockwise90:
        return StructurePos{zeroPos.x + mirrorDeltaZ, zeroPos.y, zeroPos.z + sizeX - mirrorDeltaX};
    case StructureRotation::Clockwise90:
        return StructurePos{zeroPos.x + sizeZ - mirrorDeltaZ, zeroPos.y, zeroPos.z + mirrorDeltaX};
    case StructureRotation::Clockwise180:
        return StructurePos{zeroPos.x + sizeX - mirrorDeltaX, zeroPos.y, zeroPos.z + sizeZ - mirrorDeltaZ};
    case StructureRotation::None:
    default:
        return StructurePos{zeroPos.x + mirrorDeltaX, zeroPos.y, zeroPos.z + mirrorDeltaZ};
    }
}

} // namespace basalt::server::world::structure
