#include <basalt/server/world/terrain/surface/ChunkBiomeEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::surface {

ChunkBiomeEnvironment::ChunkBiomeEnvironment(const SurfaceRuleEnvironment& delegate,
                                             densityfunction::SamplerContext* context)
    : delegate_(&delegate), context_(context) {
    if (const std::optional<std::int64_t> seed = delegate.biomeZoomSeed(); seed.has_value()) {
        zoomer_.emplace(*seed);
    }
}

const std::string& ChunkBiomeEnvironment::biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
    if (!zoomer_.has_value()) {
        return delegate_->biomeAt(x, y, z, context_);
    }
    return zoomer_->biomeAt(
        x, y, z, [this](std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) -> const std::string& {
            return noiseBiomeAt(quartX, quartY, quartZ, context_);
        });
}

const std::string& ChunkBiomeEnvironment::noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                       densityfunction::SamplerContext*) const {
    const std::int64_t key = (static_cast<std::int64_t>(quartX) & 0x1FFFFFLL) |
                             ((static_cast<std::int64_t>(quartY) & 0xFFFFFLL) << 21) |
                             ((static_cast<std::int64_t>(quartZ) & 0x1FFFFFLL) << 41);
    const auto found = noiseBiomes_.find(key);
    if (found != noiseBiomes_.end()) {
        return *found->second;
    }
    const std::string& named = delegate_->noiseBiomeAt(quartX, quartY, quartZ, context_);
    noiseBiomes_.emplace(key, &named);
    return named;
}

} // namespace basalt::server::world::terrain::surface
