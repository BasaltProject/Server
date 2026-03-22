#include <basalt/server/world/densityfunction/SamplerContext.hpp>
#include <basalt/server/world/terrain/TheEndBiomeSource.hpp>

#include <cstdint>

namespace basalt::server::world::terrain {
namespace {

constexpr std::int64_t kIslandChunkDistanceSquared = 4096;
constexpr double kHighlandsThreshold = 0.25;
constexpr double kMidlandsThreshold = -0.0625;
constexpr double kSmallIslandsThreshold = -0.21875;

} // namespace

std::string_view TheEndBiomeSource::biomeAtQuart(const std::int32_t quartX, const std::int32_t quartY,
                                                 const std::int32_t quartZ) const {
    const std::int32_t blockX = quartX << 2;
    const std::int32_t blockY = quartY << 2;
    const std::int32_t blockZ = quartZ << 2;
    const std::int32_t chunkX = blockX >> 4;
    const std::int32_t chunkZ = blockZ >> 4;
    if (static_cast<std::int64_t>(chunkX) * chunkX + static_cast<std::int64_t>(chunkZ) * chunkZ <=
        kIslandChunkDistanceSquared) {
        return "minecraft:the_end";
    }

    const std::int32_t centerX = (chunkX * 2 + 1) * 8;
    const std::int32_t centerZ = (chunkZ * 2 + 1) * 8;
    densityfunction::SamplerContext context{densityfunction::SamplerContext::Options{}};
    const auto heightValue = static_cast<double>(erosion_->sampleValue(context, centerX, blockY, centerZ));
    if (heightValue > kHighlandsThreshold) {
        return "minecraft:end_highlands";
    }
    if (heightValue >= kMidlandsThreshold) {
        return "minecraft:end_midlands";
    }
    if (heightValue < kSmallIslandsThreshold) {
        return "minecraft:small_end_islands";
    }
    return "minecraft:end_barrens";
}

std::string_view TheEndBiomeSource::blockBiomeAt(const std::int32_t x, const std::int32_t y,
                                                 const std::int32_t z) const {
    if (!zoomer_.has_value()) {
        return biomeAtQuart(x >> 2, y >> 2, z >> 2);
    }
    return zoomer_->biomeAt(x, y, z, [this](std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) {
        return biomeAtQuart(quartX, quartY, quartZ);
    });
}

} // namespace basalt::server::world::terrain
