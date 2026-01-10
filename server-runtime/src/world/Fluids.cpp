#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::world::fluids {

double eyeHeight(bool sneaking) { return sneaking ? 1.27 : 1.62; }

Submersion of(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr || block->namespaceName() != "minecraft") {
        return Submersion::None;
    }
    const std::string_view path = block->path();
    if (path == "water") {
        return Submersion::Water;
    }
    if (path == "lava") {
        return Submersion::Lava;
    }
    return Submersion::None;
}

std::int32_t airAfterTick(std::int32_t air, bool eyesInWater) {
    return eyesInWater ? air - 1 : std::min(air + 4, kMaxAir);
}

} // namespace basalt::server::runtime::world::fluids
