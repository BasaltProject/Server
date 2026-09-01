#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/StructurePieceFill.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>

#include <map>
#include <string>
#include <utility>

namespace basalt::server::world::structure {
namespace {

/// parity: the mirror and rotation `setOrientation` derives from a piece's facing.
[[nodiscard]] StructureMirror mirrorFor(std::optional<StructureDirection> orientation) {
    if (!orientation.has_value()) {
        return StructureMirror::None;
    }
    return *orientation == StructureDirection::South || *orientation == StructureDirection::West
               ? StructureMirror::LeftRight
               : StructureMirror::None;
}

[[nodiscard]] StructureRotation rotationFor(std::optional<StructureDirection> orientation) {
    if (!orientation.has_value()) {
        return StructureRotation::None;
    }
    return *orientation == StructureDirection::West || *orientation == StructureDirection::East
               ? StructureRotation::Clockwise90
               : StructureRotation::None;
}

} // namespace

StructurePieceFill::StructurePieceFill(StructureBoundingBox box, std::optional<StructureDirection> orientation,
                                       Chunk& chunk, StructureBoundingBox clip, ReplacePredicate canBeReplaced)
    : box_(box), orientation_(orientation), chunk_(&chunk), clip_(clip), canBeReplaced_(std::move(canBeReplaced)),
      mirror_(mirrorFor(orientation)), rotation_(rotationFor(orientation)) {}

std::int32_t StructurePieceFill::worldX(std::int32_t x, std::int32_t z) const {
    if (!orientation_.has_value()) {
        return x;
    }
    switch (*orientation_) {
    case StructureDirection::North:
    case StructureDirection::South:
        return box_.minX + x;
    case StructureDirection::West:
        return box_.maxX - z;
    case StructureDirection::East:
        return box_.minX + z;
    default:
        return x;
    }
}

std::int32_t StructurePieceFill::worldY(std::int32_t y) const { return orientation_.has_value() ? y + box_.minY : y; }

std::int32_t StructurePieceFill::worldZ(std::int32_t x, std::int32_t z) const {
    if (!orientation_.has_value()) {
        return z;
    }
    switch (*orientation_) {
    case StructureDirection::North:
        return box_.maxZ - z;
    case StructureDirection::South:
        return box_.minZ + z;
    case StructureDirection::West:
    case StructureDirection::East:
        return box_.minZ + x;
    default:
        return z;
    }
}

std::int32_t StructurePieceFill::blockAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const StructurePos position{worldX(x, z), worldY(y), worldZ(x, z)};
    if (!clip_.isInside(position)) {
        return registry::blocks::air().stateId;
    }
    return chunk_->decorationBlockStateId(position.x, position.y, position.z);
}

void StructurePieceFill::place(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) const {
    const StructurePos position{worldX(x, z), worldY(y), worldZ(x, z)};
    if (!clip_.isInside(position)) {
        return;
    }
    const std::int32_t existing = chunk_->decorationBlockStateId(position.x, position.y, position.z);
    if (canBeReplaced_ && !canBeReplaced_(existing)) {
        return;
    }
    chunk_->setDecorationBlockStateId(position.x, position.y, position.z, oriented(stateId), existing);
}

std::int32_t StructurePieceFill::oriented(std::int32_t stateId) const {
    if (!orientation_.has_value() || *orientation_ == StructureDirection::North) {
        return stateId;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    if (properties.empty()) {
        return stateId;
    }
    return StructureTemplate::transformedStateId(stateId, properties, mirror_, rotation_);
}

void StructurePieceFill::placeEntity(const wire::common::Identifier& type, std::int32_t x, std::int32_t y,
                                     std::int32_t z, wire::nbt::NbtCompound data, double yOffset) const {
    const StructurePos position{worldX(x, z), worldY(y), worldZ(x, z)};
    if (!clip_.isInside(position)) {
        return;
    }
    chunk_->addGeneratedEntity(ChunkGeneratedEntity{.type = type,
                                                    .x = position.x + 0.5,
                                                    .y = position.y + yOffset,
                                                    .z = position.z + 0.5,
                                                    .yaw = 0.0F,
                                                    .pitch = 0.0F,
                                                    .data = std::move(data)});
}

bool StructurePieceFill::isInterior(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const StructurePos position{worldX(x, z), worldY(y + 1), worldZ(x, z)};
    if (!clip_.isInside(position)) {
        return false;
    }
    return position.y < chunk_->worldgenHeightAt(position.x & 15, position.z & 15, HeightmapType::OceanFloor);
}

void StructurePieceFill::generateBox(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1,
                                     std::int32_t y1, std::int32_t z1, std::int32_t edge, std::int32_t fill,
                                     bool skipAir) const {
    for (std::int32_t y = y0; y <= y1; ++y) {
        for (std::int32_t x = x0; x <= x1; ++x) {
            for (std::int32_t z = z0; z <= z1; ++z) {
                if (skipAir && registry::blocks::isAir(blockAt(x, y, z))) {
                    continue;
                }
                const bool inside = y != y0 && y != y1 && x != x0 && x != x1 && z != z0 && z != z1;
                place(inside ? fill : edge, x, y, z);
            }
        }
    }
}

void StructurePieceFill::generateMaybeBox(random::RandomSource& random, float probability, std::int32_t x0,
                                          std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                                          std::int32_t z1, std::int32_t edge, std::int32_t fill, bool skipAir,
                                          bool onlyInside) const {
    for (std::int32_t y = y0; y <= y1; ++y) {
        for (std::int32_t x = x0; x <= x1; ++x) {
            for (std::int32_t z = z0; z <= z1; ++z) {
                if (random.nextFloat() > probability) {
                    continue;
                }
                if (skipAir && registry::blocks::isAir(blockAt(x, y, z))) {
                    continue;
                }
                if (onlyInside && !isInterior(x, y, z)) {
                    continue;
                }
                const bool inside = y != y0 && y != y1 && x != x0 && x != x1 && z != z0 && z != z1;
                place(inside ? fill : edge, x, y, z);
            }
        }
    }
}

void StructurePieceFill::generateUpperHalfSphere(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1,
                                                 std::int32_t y1, std::int32_t z1, std::int32_t fill,
                                                 bool skipAir) const {
    const float diagX = static_cast<float>(x1 - x0 + 1);
    const float diagY = static_cast<float>(y1 - y0 + 1);
    const float diagZ = static_cast<float>(z1 - z0 + 1);
    const float centreX = static_cast<float>(x0) + diagX / 2.0F;
    const float centreZ = static_cast<float>(z0) + diagZ / 2.0F;
    for (std::int32_t y = y0; y <= y1; ++y) {
        const float dy = static_cast<float>(y - y0) / diagY;
        for (std::int32_t x = x0; x <= x1; ++x) {
            const float dx = (static_cast<float>(x) - centreX) / (diagX * 0.5F);
            for (std::int32_t z = z0; z <= z1; ++z) {
                const float dz = (static_cast<float>(z) - centreZ) / (diagZ * 0.5F);
                if (skipAir && registry::blocks::isAir(blockAt(x, y, z))) {
                    continue;
                }
                if (dx * dx + dy * dy + dz * dz <= 1.05F) {
                    place(fill, x, y, z);
                }
            }
        }
    }
}

void StructurePieceFill::maybePlace(random::RandomSource& random, float probability, std::int32_t x, std::int32_t y,
                                    std::int32_t z, std::int32_t stateId) const {
    if (random.nextFloat() < probability) {
        place(stateId, x, y, z);
    }
}

} // namespace basalt::server::world::structure
