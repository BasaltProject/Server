#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/FossilFeature.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace basalt::server::world::terrain::decoration {
namespace {

using structure::StructureBoundingBox;
using structure::StructureMirror;
using structure::StructurePos;
using structure::StructureRotation;
using structure::StructureTemplate;

constexpr std::int32_t kBurialDepth = 15;
constexpr std::int32_t kBurialJitter = 10;
constexpr std::array<StructureRotation, 4> kRotations{StructureRotation::None, StructureRotation::Clockwise90,
                                                      StructureRotation::Clockwise180,
                                                      StructureRotation::Counterclockwise90};

/// parity: `StructureTemplate.getSize(rotation)`, which swaps X and Z on a quarter turn.
[[nodiscard]] StructurePos rotatedSize(const StructureTemplate& template_, StructureRotation rotation) {
    if (rotation == StructureRotation::Clockwise90 || rotation == StructureRotation::Counterclockwise90) {
        return StructurePos{template_.size().z, template_.size().y, template_.size().x};
    }
    return template_.size();
}

/// parity: `getZeroPositionWithTransform` with no mirror, which only ever offsets.
[[nodiscard]] StructurePos zeroPositionWithTransform(StructurePos zeroPos, StructureRotation rotation,
                                                     const StructureTemplate& template_) {
    const std::int32_t sizeX = template_.size().x - 1;
    const std::int32_t sizeZ = template_.size().z - 1;
    switch (rotation) {
    case StructureRotation::Counterclockwise90:
        return StructurePos{zeroPos.x, zeroPos.y, zeroPos.z + sizeX};
    case StructureRotation::Clockwise90:
        return StructurePos{zeroPos.x + sizeZ, zeroPos.y, zeroPos.z};
    case StructureRotation::Clockwise180:
        return StructurePos{zeroPos.x + sizeX, zeroPos.y, zeroPos.z + sizeZ};
    case StructureRotation::None:
    default:
        return zeroPos;
    }
}

/// parity: `countEmptyCorners` over the eight corners of the box and nothing between them. Air, lava
/// and water each count as empty.
[[nodiscard]] std::int32_t emptyCorners(const Chunk& chunk, const StructureBoundingBox& box) {
    const ChunkHeight bounds = chunk.verticalBounds();
    std::int32_t count = 0;
    for (const std::int32_t x : {box.maxX, box.minX}) {
        for (const std::int32_t y : {box.maxY, box.minY}) {
            for (const std::int32_t z : {box.maxZ, box.minZ}) {
                if (y < bounds.minY() || y > bounds.maxY()) {
                    ++count;
                    continue;
                }
                const std::int32_t state = chunk.decorationBlockStateId(x, y, z);
                const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(state);
                const std::string path = block == nullptr ? std::string{} : block->path();
                if (registry::blocks::isAir(state) || path == "lava" || path == "water") {
                    ++count;
                }
            }
        }
    }
    return count;
}

} // namespace

bool fossilFeature::place(Chunk& chunk, random::RandomSource& random, const StructurePos origin, const Spec& spec,
                          const TemplateLookup& templates, const OceanFloorAt& oceanFloorAt) {
    const StructureRotation rotation =
        kRotations[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kRotations.size())))];
    const std::int32_t index = random.nextInt(static_cast<std::int32_t>(spec.structures.size()));
    const StructureTemplate* base = templates(spec.structures[static_cast<std::size_t>(index)]);
    const StructureTemplate* overlay = templates(spec.overlays[static_cast<std::size_t>(index)]);
    if (base == nullptr || overlay == nullptr) {
        return false;
    }

    const StructurePos size = rotatedSize(*base, rotation);
    const StructurePos lowCorner{origin.x - size.x / 2, origin.y, origin.z - size.z / 2};
    std::int32_t lowestSurfaceY = origin.y;
    for (std::int32_t scanX = 0; scanX < size.x; ++scanX) {
        for (std::int32_t scanZ = 0; scanZ < size.z; ++scanZ) {
            const std::optional<std::int32_t> height = oceanFloorAt(lowCorner.x + scanX, lowCorner.z + scanZ);
            if (height.has_value() && *height < lowestSurfaceY) {
                lowestSurfaceY = *height;
            }
        }
    }
    const std::int32_t targetY =
        std::max(lowestSurfaceY - kBurialDepth - random.nextInt(kBurialJitter), chunk.verticalBounds().minY() + 10);
    const StructurePos targetPos =
        zeroPositionWithTransform(StructurePos{lowCorner.x, targetY, lowCorner.z}, rotation, *base);

    const StructurePos pivot{0, 0, 0};
    const StructureBoundingBox box = base->getBoundingBox(targetPos, rotation, pivot, StructureMirror::None);
    if (emptyCorners(chunk, box) > spec.maxEmptyCornersAllowed) {
        return false;
    }

    // parity: the level bounding box is the chunk grown by 16 on each side, so a fossil that starts
    // here may reach a neighbour; what lands there is deferred like any other write.
    const ChunkHeight bounds = chunk.verticalBounds();
    structure::TemplatePlacement placement;
    placement.position = targetPos;
    placement.rotation = rotation;
    placement.mirror = StructureMirror::None;
    placement.pivot = pivot;
    placement.clipTo = StructureBoundingBox{chunk.chunkX() * 16 - 16, bounds.minY(), chunk.chunkZ() * 16 - 16,
                                            chunk.chunkX() * 16 + 31, bounds.maxY(), chunk.chunkZ() * 16 + 31};
    placement.processors = &spec.fossilProcessors;
    // parity: the feature calls `setRandom`, so the palette pick and the rot draw from this stream
    // in template order rather than from a fresh source per position.
    placement.settingsRandom = &random;
    base->placeInto(chunk, placement);
    placement.processors = &spec.overlayProcessors;
    overlay->placeInto(chunk, placement);
    return true;
}

} // namespace basalt::server::world::terrain::decoration
