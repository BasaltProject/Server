#include "NetherVegetalShared.hpp"

#include <string>

namespace basalt::server::world::terrain::decoration::nether::shared {
namespace {

constexpr std::int32_t kPatchAttempts = 96;

const PathSet& normalMushroomBiomes() {
    static const PathSet paths = {"nether_wastes", "crimson_forest", "warped_forest"};
    return paths;
}

/// parity: the mushroom's own placement rule, which accepts a handful of blocks whatever the light
/// is and everything else only if it is solid.
[[nodiscard]] bool supportsMushroom(const std::string& path) {
    static const PathSet lightOverride = {"mycelium", "podzol", "crimson_nylium", "warped_nylium"};
    static const PathSet solidSupport = {
        "netherrack", "basalt",      "blackstone",      "bedrock",           "soul_sand",     "soul_soil",
        "gravel",     "magma_block", "nether_gold_ore", "nether_quartz_ore", "ancient_debris"};
    return lightOverride.contains(path) || solidSupport.contains(path);
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

} // namespace

void placeNormalMushroom(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource,
                         const std::int32_t rarity, const std::string_view mushroom) {
    if (random.nextFloat() >= 1.0F / static_cast<float>(rarity)) {
        return;
    }
    const std::int32_t baseX = chunk.chunkX() * 16 + random.nextInt(16);
    const std::int32_t baseZ = chunk.chunkZ() * 16 + random.nextInt(16);
    const std::int32_t baseY = chunk.heightAt(baseX & 15, baseZ & 15, HeightmapType::MotionBlocking);
    if (!normalMushroomBiomes().contains(biomePath(biomeSource(baseX, baseY, baseZ, nullptr)))) {
        return;
    }
    const std::int32_t mushroomState = stateId(mushroom);
    for (std::int32_t attempt = 0; attempt < kPatchAttempts; ++attempt) {
        const std::int32_t x = baseX + triangle(random, 7);
        const std::int32_t y = baseY + triangle(random, 3);
        const std::int32_t z = baseZ + triangle(random, 7);
        const std::string support = blockPathAt(chunk, x, y - 1, z);
        if (isAir(chunk, x, y, z) && !support.empty() && supportsMushroom(support)) {
            setBlock(chunk, x, y, z, mushroomState);
        }
    }
}

} // namespace basalt::server::world::terrain::decoration::nether::shared
