#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/NetherFortressBrush.hpp>

#include <string>
#include <utility>

namespace basalt::server::world::terrain::netherFortressPieces {
namespace {

using structure::StructureDirection;
using structure::StructurePos;

} // namespace

std::int32_t stateOf(std::string_view path, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> found =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla(std::string{path}), properties);
    if (found.has_value()) {
        return *found;
    }
    const registry::BlockState* plain = registry::blocks::named(path);
    return plain == nullptr ? registry::blocks::air().stateId : plain->stateId;
}

std::int32_t Brush::worldX(std::int32_t x, std::int32_t z) const {
    switch (piece_->orientation) {
    case StructureDirection::West:
        return piece_->boundingBox.maxX - z;
    case StructureDirection::East:
        return piece_->boundingBox.minX + z;
    case StructureDirection::North:
    case StructureDirection::South:
        return piece_->boundingBox.minX + x;
    default:
        return x;
    }
}

std::int32_t Brush::worldZ(std::int32_t x, std::int32_t z) const {
    switch (piece_->orientation) {
    case StructureDirection::North:
        return piece_->boundingBox.maxZ - z;
    case StructureDirection::South:
        return piece_->boundingBox.minZ + z;
    case StructureDirection::West:
    case StructureDirection::East:
        return piece_->boundingBox.minZ + x;
    default:
        return z;
    }
}

StructureDirection Brush::transformDirection(StructureDirection direction) const {
    StructureDirection transformed = direction;
    if (piece_->orientation == StructureDirection::South || piece_->orientation == StructureDirection::West) {
        if (transformed == StructureDirection::North) {
            transformed = StructureDirection::South;
        } else if (transformed == StructureDirection::South) {
            transformed = StructureDirection::North;
        }
    }
    if (piece_->orientation == StructureDirection::West || piece_->orientation == StructureDirection::East) {
        transformed = structure::rotated(transformed, structure::StructureRotation::Clockwise90);
    }
    return transformed;
}

std::int32_t Brush::fence(std::initializer_list<StructureDirection> connections) const {
    bool north = false;
    bool south = false;
    bool west = false;
    bool east = false;
    for (const StructureDirection connection : connections) {
        switch (transformDirection(connection)) {
        case StructureDirection::North:
            north = true;
            break;
        case StructureDirection::South:
            south = true;
            break;
        case StructureDirection::West:
            west = true;
            break;
        case StructureDirection::East:
            east = true;
            break;
        default:
            break;
        }
    }
    return stateOf("nether_brick_fence", {{"east", east ? "true" : "false"},
                                          {"north", north ? "true" : "false"},
                                          {"south", south ? "true" : "false"},
                                          {"waterlogged", "false"},
                                          {"west", west ? "true" : "false"}});
}

std::int32_t Brush::stairs(StructureDirection facing) const {
    return stateOf("nether_brick_stairs",
                   {{"facing", std::string{structure::serializedName(transformDirection(facing))}},
                    {"half", "bottom"},
                    {"shape", "straight"},
                    {"waterlogged", "false"}});
}

void Brush::box(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1, std::int32_t z1,
                std::int32_t stateId) const {
    for (std::int32_t y = y0; y <= y1; ++y) {
        for (std::int32_t x = x0; x <= x1; ++x) {
            for (std::int32_t z = z0; z <= z1; ++z) {
                placeBlock(x, y, z, stateId);
            }
        }
    }
}

void Brush::placeBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) const {
    const std::int32_t wx = worldX(x, z);
    const std::int32_t wy = piece_->boundingBox.minY + y;
    const std::int32_t wz = worldZ(x, z);
    if (wy < chunk_->verticalBounds().minY() || wy > chunk_->verticalBounds().maxY()) {
        return;
    }
    if (!clip_.isInside(StructurePos{wx, wy, wz})) {
        return;
    }
    chunk_->setBlockStateId(wx & 15, wy, wz & 15, stateId);
}

void Brush::fillDown(std::int32_t x, std::int32_t startY, std::int32_t z, std::int32_t stateId) const {
    const std::int32_t wx = worldX(x, z);
    const std::int32_t wz = worldZ(x, z);
    std::int32_t wy = piece_->boundingBox.minY + startY;
    if (!clip_.isInside(StructurePos{wx, wy, wz})) {
        return;
    }
    while (wy > chunk_->verticalBounds().minY() + 1) {
        if (!replaceableByStructures(chunk_->blockStateId(wx & 15, wy, wz & 15))) {
            break;
        }
        chunk_->setBlockStateId(wx & 15, wy, wz & 15, stateId);
        --wy;
    }
}

bool Brush::isInside(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return clip_.isInside(StructurePos{worldX(x, z), piece_->boundingBox.minY + y, worldZ(x, z)});
}

std::int32_t Brush::blockAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return chunk_->blockStateId(worldX(x, z) & 15, piece_->boundingBox.minY + y, worldZ(x, z) & 15);
}

bool Brush::isChestAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(blockAt(x, y, z));
    return block != nullptr && block->path() == "chest";
}

} // namespace basalt::server::world::terrain::netherFortressPieces
