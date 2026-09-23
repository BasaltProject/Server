#include <basalt/server/runtime/world/NetherSurfaceEnvironment.hpp>

namespace basalt::server::runtime::world {

NetherSurfaceEnvironment::NetherSurfaceEnvironment(const server::world::terrain::NetherBiomeSource& biomeSource,
                                                   std::int64_t biomeZoomSeed)
    : biomeZoomSeed_(biomeZoomSeed), biomeSource_(&biomeSource) {}

const std::string& NetherSurfaceEnvironment::nameOf(std::string_view biome) const {
    const std::scoped_lock lock{mutex_};
    const auto known = biomeNames_.find(biome);
    if (known != biomeNames_.end()) {
        return known->second;
    }
    return biomeNames_.emplace(std::string{biome}, std::string{biome}).first->second;
}

const std::string& NetherSurfaceEnvironment::biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return biomeAt(x, y, z, nullptr);
}

const std::string& NetherSurfaceEnvironment::biomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                                     server::world::densityfunction::SamplerContext* context) const {
    return nameOf(biomeSource_->blockBiomeAt(x, y, z, context));
}

const std::string&
NetherSurfaceEnvironment::noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                       server::world::densityfunction::SamplerContext* context) const {
    return nameOf(biomeSource_->biomeAtQuart(quartX, quartY, quartZ, context));
}

} // namespace basalt::server::runtime::world
