#include "AquaticSupport.hpp"

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>

#include <span>

namespace basalt::server::world::terrain::decoration::aquaticsupport {
namespace {

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) { return registry::blocks::named(path)->stateId; }

} // namespace

bool seagrassSupport(const std::int32_t below) {
    return below != stateIdOf("magma_block") && registry::blockShapes::isFaceSturdy(below, registry::BlockFace::Up);
}

bool seaPickleSupport(const std::int32_t below) {
    // The boxes are flattened six numbers each; index 4 of a box is its top edge.
    const std::span<const std::int32_t> boxes = registry::blockShapes::collisionBoxes(below);
    for (std::size_t offset = 4; offset < boxes.size(); offset += 6) {
        if (boxes[offset] == registry::blockShapes::kUnits) {
            return true;
        }
    }
    return registry::blockShapes::isFaceSturdy(below, registry::BlockFace::Up);
}

bool kelpSupport(const std::int32_t below) {
    if (below == stateIdOf("magma_block")) {
        return false;
    }
    return below == stateIdOf("kelp_plant") || registry::blockStateFamilies::isKelpState(below) ||
           registry::blockShapes::isFaceSturdy(below, registry::BlockFace::Up);
}

} // namespace basalt::server::world::terrain::decoration::aquaticsupport
