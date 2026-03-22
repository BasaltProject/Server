#include <basalt/server/world/climate/OverworldBiomes.hpp>
#include <basalt/server/world/terrain/NetherBiomeSource.hpp>

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain {
namespace {

using climate::BiomeName;

/// parity: the five Nether parameter points, offsets included.
[[nodiscard]] std::vector<std::pair<climate::ParameterPoint, BiomeName>> netherParameters() {
    return {{climate::parameters(0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F), "minecraft:nether_wastes"},
            {climate::parameters(0.0F, -0.5F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F), "minecraft:soul_sand_valley"},
            {climate::parameters(0.4F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F), "minecraft:crimson_forest"},
            {climate::parameters(0.0F, 0.5F, 0.0F, 0.0F, 0.0F, 0.0F, 0.375F), "minecraft:warped_forest"},
            {climate::parameters(-0.5F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.175F), "minecraft:basalt_deltas"}};
}

} // namespace

struct NetherBiomeSource::State {
    std::unique_ptr<const climate::RTree<BiomeName>> parameters;
    climate::Sampler sampler;
    std::optional<BiomeZoomer> zoomer;
};

NetherBiomeSource::NetherBiomeSource(const NoiseRouter& router, const std::optional<std::int64_t> biomeZoomSeed) {
    auto state = std::make_shared<State>(State{
        .parameters = climate::RTree<BiomeName>::create(netherParameters()),
        .sampler = climate::Sampler{*router.state, router.temperature, router.vegetation, router.continents,
                                    router.erosion, router.depth, router.ridges},
        .zoomer = biomeZoomSeed.has_value() ? std::optional<BiomeZoomer>{BiomeZoomer{*biomeZoomSeed}} : std::nullopt});
    state_ = std::move(state);
}

std::string_view NetherBiomeSource::biomeAtQuart(const std::int32_t quartX, const std::int32_t quartY,
                                                 const std::int32_t quartZ,
                                                 densityfunction::SamplerContext* cache) const {
    return state_->parameters->search(state_->sampler.sample(quartX, quartY, quartZ, cache).toArray());
}

std::string_view NetherBiomeSource::biomeAt(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                            densityfunction::SamplerContext* cache) const {
    return biomeAtQuart(x >> 2, y >> 2, z >> 2, cache);
}

std::string_view NetherBiomeSource::blockBiomeAt(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                                 densityfunction::SamplerContext* cache) const {
    if (!state_->zoomer.has_value()) {
        return biomeAt(x, y, z, cache);
    }
    return state_->zoomer->biomeAt(x, y, z, [&](std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) {
        return biomeAtQuart(quartX, quartY, quartZ, cache);
    });
}

NetherBiomeLookup NetherBiomeSource::lookup() const {
    return [state = state_, self = *this](std::int32_t x, std::int32_t y, std::int32_t z,
                                          densityfunction::SamplerContext* cache) {
        return self.blockBiomeAt(x, y, z, cache);
    };
}

NetherBiomeLookup NetherBiomeSource::cached(densityfunction::SamplerContext& context) const {
    if (!state_->zoomer.has_value()) {
        return lookup();
    }
    // One worker's own quart cache: the zoom asks for the same quart from many block positions.
    auto noiseBiomes = std::make_shared<std::map<std::int64_t, BiomeName>>();
    return [self = *this, noiseBiomes, fallback = &context](std::int32_t x, std::int32_t y, std::int32_t z,
                                                            densityfunction::SamplerContext* cache) {
        densityfunction::SamplerContext* activeCache = cache != nullptr ? cache : fallback;
        return self.state_->zoomer->biomeAt(
            x, y, z, [&](std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) -> BiomeName {
                const std::int64_t key = (static_cast<std::int64_t>(quartX) & 0x1FFFFFLL) |
                                         ((static_cast<std::int64_t>(quartY) & 0xFFFFFLL) << 21) |
                                         ((static_cast<std::int64_t>(quartZ) & 0x1FFFFFLL) << 41);
                const auto found = noiseBiomes->find(key);
                if (found != noiseBiomes->end()) {
                    return found->second;
                }
                const BiomeName biome = self.biomeAtQuart(quartX, quartY, quartZ, activeCache);
                noiseBiomes->emplace(key, biome);
                return biome;
            });
    };
}

} // namespace basalt::server::world::terrain
