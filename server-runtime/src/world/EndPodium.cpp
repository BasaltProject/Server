#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/EndPodium.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world::endPodium {
namespace {

using wire::common::Identifier;

constexpr double kRimRadius = 2.5;
constexpr double kOuterRadius = 3.5;
constexpr std::int32_t kPillarHeight = 4;
constexpr std::int32_t kCeiling = 32;
constexpr std::int32_t kLowestOriginY = 63;

/// parity: the four torches on the pillar, each facing out of the side it hangs on.
struct Torch {
    std::int32_t x = 0;
    std::int32_t z = 0;
    std::string_view facing;
};

constexpr std::array<Torch, 4> kTorches{{
    {0, -1, "north"},
    {0, 1, "south"},
    {-1, 0, "west"},
    {1, 0, "east"},
}};

[[nodiscard]] std::int32_t stateOf(std::string_view path) {
    const registry::BlockState* block = registry::blocks::named(path);
    return block == nullptr ? registry::blocks::air().stateId : block->stateId;
}

[[nodiscard]] std::int32_t endPortal() {
    static const std::int32_t stateId = stateOf("end_portal");
    return stateId;
}

[[nodiscard]] std::int32_t endStone() {
    static const std::int32_t stateId = stateOf("end_stone");
    return stateId;
}

[[nodiscard]] std::int32_t wallTorch(std::string_view facing) {
    const std::optional<std::int32_t> stateId =
        registry::blocks::stateIdOf(Identifier::vanilla("wall_torch"), {{"facing", std::string{facing}}});
    return stateId.value_or(registry::blocks::air().stateId);
}

/// parity: `EnderDragonFight.spawnExitPortal`: the surface at the origin, one block down, then down
/// again through any bedrock, and never below the bottom of the world.
[[nodiscard]] std::int32_t originY(WorldState& world, server::world::ChunkHeight bounds) {
    const server::world::Chunk& chunk = world.getOrGenerateChunk(0, 0);
    std::int32_t y = chunk.heightAt(0, 0, server::world::HeightmapType::MotionBlockingNoLeaves) - 1;
    while (world.blockState(0, y, 0) == registry::blocks::bedrock().stateId && y > kLowestOriginY) {
        --y;
    }
    return std::max(bounds.minY() + 1, y);
}

[[nodiscard]] std::array<std::int32_t, 3> place(WorldState& world, std::int32_t origin, bool active) {
    const std::int32_t bedrock = registry::blocks::bedrock().stateId;
    const std::int32_t air = registry::blocks::air().stateId;
    for (std::int32_t x = -kPodiumRadius; x <= kPodiumRadius; ++x) {
        for (std::int32_t z = -kPodiumRadius; z <= kPodiumRadius; ++z) {
            const double distance = std::sqrt(static_cast<double>(x * x + z * z));
            const bool insideRim = distance <= kRimRadius;
            if (!insideRim && distance > kOuterRadius) {
                continue;
            }
            for (std::int32_t y = origin - 1; y <= origin + kCeiling; ++y) {
                std::int32_t stateId = air;
                if (y < origin) {
                    stateId = insideRim ? bedrock : endStone();
                } else if (y == origin) {
                    stateId = !insideRim ? bedrock : (active ? endPortal() : air);
                }
                world.setBlockState(x, y, z, stateId);
            }
        }
    }
    for (std::int32_t height = 0; height < kPillarHeight; ++height) {
        world.setBlockState(0, origin + height, 0, bedrock);
    }
    const std::int32_t torchY = origin + 2;
    for (const Torch& torch : kTorches) {
        world.setBlockState(torch.x, torchY, torch.z, wallTorch(torch.facing));
    }
    return {0, origin, 0};
}

} // namespace

bool exists(const WorldState& world, server::world::ChunkHeight bounds) {
    for (std::int32_t y = bounds.minY(); y <= bounds.maxY(); ++y) {
        if (world.blockState(0, y, 0) == registry::blocks::bedrock().stateId) {
            return true;
        }
    }
    return false;
}

std::optional<std::array<std::int32_t, 3>> ensure(WorldState& world, bool active, server::world::ChunkHeight bounds) {
    for (std::int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
        for (std::int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
            world.getOrGenerateChunk(chunkX, chunkZ);
        }
    }
    if (exists(world, bounds)) {
        return std::nullopt;
    }
    return place(world, originY(world, bounds), active);
}

} // namespace basalt::server::runtime::world::endPodium
