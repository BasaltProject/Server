#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
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

namespace {

/// parity: `FlowingFluid.getOwnHeight` and `getHeight`.
[[nodiscard]] double lavaHeight(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                std::int32_t stateId) {
    const std::optional<std::int32_t> above = world.blockState(x, y + 1, z);
    if (above.has_value() && of(*above) == Submersion::Lava) {
        return 1.0;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto level = properties.find("level");
    const std::int32_t value = level == properties.end() ? 0 : std::stoi(level->second);
    const std::int32_t amount = value == 0 || value >= 8 ? 8 : 8 - value;
    return static_cast<double>(amount) / 9.0;
}

} // namespace

bool touchesLava(WorldState& world, const entity::ServerEntity& entity) {
    constexpr double kDeflate = 0.001;
    const double half = (entity.width() / 2.0) - kDeflate;
    const double minY = entity.y + kDeflate;
    const double maxY = entity.y + entity.height() - kDeflate;
    for (auto x = static_cast<std::int32_t>(std::floor(entity.x - half));
         x <= static_cast<std::int32_t>(std::floor(entity.x + half)); ++x) {
        for (auto y = static_cast<std::int32_t>(std::floor(minY)); y <= static_cast<std::int32_t>(std::floor(maxY));
             ++y) {
            for (auto z = static_cast<std::int32_t>(std::floor(entity.z - half));
                 z <= static_cast<std::int32_t>(std::floor(entity.z + half)); ++z) {
                const std::optional<std::int32_t> state = world.blockState(x, y, z);
                if (state.has_value() && of(*state) == Submersion::Lava &&
                    static_cast<double>(y) + lavaHeight(world, x, y, z, *state) >= minY) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace basalt::server::runtime::world::fluids
