#include <basalt/server/runtime/world/RealSurfaceEnvironment.hpp>

namespace basalt::server::runtime::world {

RealSurfaceEnvironment::RealSurfaceEnvironment(const server::world::terrain::NoiseRouter& router,
                                               std::int64_t biomeZoomSeed)
    : biomeZoomSeed_(biomeZoomSeed), biomeSource_(router, biomeZoomSeed) {}

const std::string& RealSurfaceEnvironment::nameOf(std::string_view biome) const {
    const std::scoped_lock lock{mutex_};
    const auto known = biomeNames_.find(biome);
    if (known != biomeNames_.end()) {
        return known->second;
    }
    std::string full{biome};
    if (full.find(':') == std::string::npos) {
        full.insert(0, "minecraft:");
    }
    return biomeNames_.emplace(std::string{biome}, std::move(full)).first->second;
}

const std::string& RealSurfaceEnvironment::biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return biomeAt(x, y, z, nullptr);
}

const std::string& RealSurfaceEnvironment::biomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                                   server::world::densityfunction::SamplerContext* context) const {
    return nameOf(biomeSource_.blockBiomeAt(x, y, z, context));
}

const std::string& RealSurfaceEnvironment::noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                        server::world::densityfunction::SamplerContext* context) const {
    return nameOf(biomeSource_.biomeAtQuart(quartX, quartY, quartZ, context));
}

} // namespace basalt::server::runtime::world
