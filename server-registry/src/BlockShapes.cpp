#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/VanillaTables.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <string_view>

namespace basalt::server::registry::blockShapes {
namespace {

constexpr std::string_view kIndexAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr std::uint32_t kFullRow = 0xFFFFFFFFU;

[[nodiscard]] std::int32_t parseInt(std::string_view text) {
    std::int32_t value = 0;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

/// One `1` per face that is set, in `BlockFace` order.
[[nodiscard]] std::int32_t faceBits(std::string_view flags) {
    std::int32_t mask = 0;
    for (std::size_t face = 0; face < flags.size(); ++face) {
        if (flags[face] == '1') {
            mask |= 1 << face;
        }
    }
    return mask;
}

/// Everything one shape says about itself, parsed once out of its encoded row.
struct Shape {
    std::int32_t dampening = 0;
    bool blocksMotion = false;
    bool replaceable = false;
    bool solid = false;
    bool solidRender = false;
    std::int32_t sturdy = 0;
    std::int32_t centre = 0;
    std::int32_t rigid = 0;
    std::int32_t occlusionFull = 0;
    /// Flattened, six numbers to a box.
    std::vector<std::int32_t> boxes;
    bool full = false;
};

using FaceMask = std::array<std::uint32_t, kUnits>;

struct Table {
    std::vector<Shape> shapes;
    std::vector<std::int32_t> byState;
    /// Every face of every shape rasterised, one 32-bit row mask per unit.
    ///
    /// Worked out up front rather than on demand: five hundred shapes by six faces is a few hundred
    /// kilobytes once, and it keeps the table immutable, which is what lets several region threads
    /// ask about a shape at the same time without a lock between them.
    std::vector<FaceMask> faces;
};

[[nodiscard]] std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    while (true) {
        const std::size_t at = text.find(separator);
        if (at == std::string_view::npos) {
            parts.push_back(text);
            return parts;
        }
        parts.push_back(text.substr(0, at));
        text = text.substr(at + 1);
    }
}

/// The columns a box covers, clipped to the face.
///
/// The clip is not defensive: a shape may reach outside its own cube (a door is forty eight units
/// tall, a piston head sticks eight into the next block) and 196 of the boxes that touch a face run
/// off it. Only the part lying on the face can cover any of it.
[[nodiscard]] std::uint32_t columnBits(std::int32_t from, std::int32_t to) {
    std::uint32_t bits = 0;
    for (std::int32_t column = std::max(from, 0); column < std::min(to, kUnits); ++column) {
        bits |= std::uint32_t{1} << column;
    }
    return bits;
}

/// One face of one shape, as row masks.
///
/// Only a box actually reaching that side of the cube contributes, the way slicing the last layer of
/// a voxel shape does.
[[nodiscard]] FaceMask rasterise(const Shape& shape, BlockFace face) {
    FaceMask mask{};
    const int axis = axisOf(face);
    const std::int32_t boundary = isPositive(face) ? kUnits : 0;
    const auto edge = static_cast<std::size_t>(isPositive(face) ? axis + 3 : axis);
    for (std::size_t offset = 0; offset < shape.boxes.size(); offset += kCorners) {
        if (shape.boxes[offset + edge] != boundary) {
            continue;
        }
        // The two axes that are not the face's own: rows and columns of its rectangle.
        const auto rowAxis = static_cast<std::size_t>(axis == 1 ? 2 : 1);
        const auto columnAxis = static_cast<std::size_t>(axis == 0 ? 2 : 0);
        const std::uint32_t columns =
            columnBits(shape.boxes[offset + columnAxis], shape.boxes[offset + columnAxis + 3]);
        const std::int32_t lastRow = std::min(shape.boxes[offset + rowAxis + 3], kUnits);
        for (std::int32_t row = std::max(shape.boxes[offset + rowAxis], 0); row < lastRow; ++row) {
            mask[static_cast<std::size_t>(row)] |= columns;
        }
    }
    return mask;
}

[[nodiscard]] const Table& table() {
    static const Table built = [] {
        Table table;
        table.shapes.reserve(registry::blockShapeSpecsTable().size());
        for (const std::string_view encoded : registry::blockShapeSpecsTable()) {
            const std::vector<std::string_view> parts = split(encoded, '|');
            Shape shape;
            shape.dampening = parseInt(parts[0]);
            shape.blocksMotion = parts[1] == "1";
            shape.replaceable = parts[2] == "1";
            shape.solid = parts[3] == "1";
            shape.solidRender = parts[4] == "1";
            shape.sturdy = faceBits(parts[5]);
            shape.centre = faceBits(parts[6]);
            shape.rigid = faceBits(parts[7]);
            shape.occlusionFull = faceBits(parts[8]);
            for (const std::string_view box : split(parts[9], ';')) {
                if (box.empty()) {
                    continue;
                }
                for (const std::string_view corner : split(box, ',')) {
                    shape.boxes.push_back(parseInt(corner));
                }
            }
            shape.full = shape.boxes.size() == kCorners;
            for (std::size_t corner = 0; corner < shape.boxes.size() && shape.full; ++corner) {
                shape.full = shape.boxes[corner] == (corner < 3 ? 0 : kUnits);
            }
            table.shapes.push_back(std::move(shape));
        }
        const std::string_view index = blockShapeIndexTable();
        table.byState.reserve(index.size() / 2);
        for (std::size_t state = 0; state * 2 + 1 < index.size(); ++state) {
            const auto high = static_cast<std::int32_t>(kIndexAlphabet.find(index[state * 2]));
            const auto low = static_cast<std::int32_t>(kIndexAlphabet.find(index[state * 2 + 1]));
            table.byState.push_back(high * static_cast<std::int32_t>(kIndexAlphabet.size()) + low);
        }
        table.faces.assign(table.shapes.size() * kBlockFaceCount, FaceMask{});
        for (std::size_t shape = 0; shape < table.shapes.size(); ++shape) {
            for (int face = 0; face < kBlockFaceCount; ++face) {
                table.faces[shape * kBlockFaceCount + static_cast<std::size_t>(face)] =
                    rasterise(table.shapes[shape], static_cast<BlockFace>(face));
            }
        }
        return table;
    }();
    return built;
}

[[nodiscard]] std::size_t shapeOf(std::int32_t stateId) {
    const Table& built = table();
    if (stateId < 0 || static_cast<std::size_t>(stateId) >= built.byState.size()) {
        return 0;
    }
    return static_cast<std::size_t>(built.byState[static_cast<std::size_t>(stateId)]);
}

[[nodiscard]] BlockFace faceOf(int axis, bool positive) {
    if (axis == 0) {
        return positive ? BlockFace::East : BlockFace::West;
    }
    if (axis == 1) {
        return positive ? BlockFace::Up : BlockFace::Down;
    }
    return positive ? BlockFace::South : BlockFace::North;
}

[[nodiscard]] const FaceMask& faceMask(std::size_t shape, BlockFace face) {
    return table().faces[shape * kBlockFaceCount + static_cast<std::size_t>(face)];
}

} // namespace

std::int32_t dampening(std::int32_t stateId) { return table().shapes[shapeOf(stateId)].dampening; }

bool blocksMotion(std::int32_t stateId) { return table().shapes[shapeOf(stateId)].blocksMotion; }

bool isReplaceable(std::int32_t stateId) { return table().shapes[shapeOf(stateId)].replaceable; }

bool isSolidState(std::int32_t stateId) { return table().shapes[shapeOf(stateId)].solid; }

bool isSolidRender(std::int32_t stateId) { return table().shapes[shapeOf(stateId)].solidRender; }

bool isFullCube(std::int32_t stateId) { return table().shapes[shapeOf(stateId)].full; }

std::span<const std::int32_t> collisionBoxes(std::int32_t stateId) { return table().shapes[shapeOf(stateId)].boxes; }

bool isEmptyShape(std::int32_t stateId) { return table().shapes[shapeOf(stateId)].boxes.empty(); }

bool isFaceSturdy(std::int32_t stateId, BlockFace face) {
    return (table().shapes[shapeOf(stateId)].sturdy & (1 << static_cast<int>(face))) != 0;
}

bool isCollisionFaceFull(std::int32_t stateId, BlockFace face) {
    return faceCovers(stateId, face, 0, kUnits, 0, kUnits);
}

bool supportsCentre(std::int32_t stateId, BlockFace face) {
    return (table().shapes[shapeOf(stateId)].centre & (1 << static_cast<int>(face))) != 0;
}

bool supportsRigid(std::int32_t stateId, BlockFace face) {
    return (table().shapes[shapeOf(stateId)].rigid & (1 << static_cast<int>(face))) != 0;
}

bool isFaceOcclusionFull(std::int32_t stateId, BlockFace face) {
    return (table().shapes[shapeOf(stateId)].occlusionFull & (1 << static_cast<int>(face))) != 0;
}

bool mergedFaceOccludes(std::int32_t sourceStateId, std::int32_t targetStateId, BlockFace face) {
    const std::size_t source = shapeOf(sourceStateId);
    const std::size_t target = shapeOf(targetStateId);
    if (table().shapes[source].full || table().shapes[target].full) {
        return true;
    }
    const std::size_t negative = isPositive(face) ? source : target;
    const std::size_t positive = isPositive(face) ? target : source;
    const int axis = axisOf(face);
    const FaceMask& negativeFace = faceMask(negative, faceOf(axis, true));
    const FaceMask& positiveFace = faceMask(positive, faceOf(axis, false));
    for (std::size_t row = 0; row < kUnits; ++row) {
        if ((negativeFace[row] | positiveFace[row]) != kFullRow) {
            return false;
        }
    }
    return true;
}

bool faceCovers(std::int32_t stateId, BlockFace face, std::int32_t minRow, std::int32_t maxRow, std::int32_t minColumn,
                std::int32_t maxColumn) {
    const FaceMask& mask = faceMask(shapeOf(stateId), face);
    const std::uint32_t columns = columnBits(minColumn, maxColumn);
    for (std::int32_t row = minRow; row < maxRow; ++row) {
        if ((mask[static_cast<std::size_t>(row)] & columns) != columns) {
            return false;
        }
    }
    return true;
}

} // namespace basalt::server::registry::blockShapes
