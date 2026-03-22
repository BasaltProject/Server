#include <basalt/server/world/climate/OverworldBiomes.hpp>
#include <basalt/server/world/terrain/OverworldBiomeSource.hpp>

namespace basalt::server::world::terrain {

OverworldBiomeSource::OverworldBiomeSource(const NoiseRouter& router, const std::optional<std::int64_t> biomeZoomSeed)
    : zoomer_(biomeZoomSeed.has_value() ? std::optional<BiomeZoomer>{BiomeZoomer{*biomeZoomSeed}} : std::nullopt),
      parameters_(climate::RTree<climate::BiomeName>::create(climate::overworldBiomes::build())),
      sampler_(*router.state, router.temperature, router.vegetation, router.continents, router.erosion, router.depth,
               router.ridges) {}

climate::BiomeName OverworldBiomeSource::biomeAtQuart(const std::int32_t quartX, const std::int32_t quartY,
                                                      const std::int32_t quartZ,
                                                      densityfunction::SamplerContext* context) const {
    return parameters_->search(sampler_.sample(quartX, quartY, quartZ, context).toArray());
}

climate::BiomeName OverworldBiomeSource::biomeAt(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                                 densityfunction::SamplerContext* context) const {
    return biomeAtQuart(x >> 2, y >> 2, z >> 2, context);
}

climate::BiomeName OverworldBiomeSource::blockBiomeAt(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                                      densityfunction::SamplerContext* context) const {
    if (!zoomer_.has_value()) {
        return biomeAt(x, y, z, context);
    }
    return zoomer_->biomeAt(x, y, z, [this, context](std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) {
        return biomeAtQuart(quartX, quartY, quartZ, context);
    });
}

} // namespace basalt::server::world::terrain
