#include <basalt/server/world/terrain/surface/ChunkBiomeEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::surface {

ChunkBiomeEnvironment::ChunkBiomeEnvironment(const SurfaceRuleEnvironment& delegate,
                                             densityfunction::SamplerContext* context, const std::int32_t chunkX,
                                             const std::int32_t chunkZ, const ChunkHeight bounds)
    : delegate_(&delegate), context_(context), windowX_((chunkX * 4) - 4), windowY_((bounds.minY() >> 2) - 2),
      windowZ_((chunkZ * 4) - 4), windowHeight_((bounds.height() / 4) + 4),
      window_(static_cast<std::size_t>(kWindowQuarts) * kWindowQuarts * static_cast<std::size_t>(windowHeight_), 0) {
    if (const std::optional<std::int64_t> seed = delegate.biomeZoomSeed(); seed.has_value()) {
        zoomer_.emplace(*seed);
    }
}

const std::string& ChunkBiomeEnvironment::biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
    if (!zoomer_.has_value()) {
        return delegate_->biomeAt(x, y, z, context_);
    }
    return zoomer_->biomeAtWith(
        x, y, z,
        [this](std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) -> const std::string& {
            return noiseBiomeAt(quartX, quartY, quartZ, context_);
        },
        [this](std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) {
            const std::int64_t key = (static_cast<std::int64_t>(quartX) & 0x1FFFFFLL) |
                                     ((static_cast<std::int64_t>(quartY) & 0xFFFFFLL) << 21) |
                                     ((static_cast<std::int64_t>(quartZ) & 0x1FFFFFLL) << 41);
            const std::uint64_t mixed = static_cast<std::uint64_t>(key) * 0x9E3779B97F4A7C15ULL;
            CornerSlot& slot = corners_[static_cast<std::size_t>(mixed >> 55)];
            if (slot.key != key) {
                slot.offsets = BiomeZoomer::offsetsAt(zoomer_->seed(), quartX, quartY, quartZ);
                slot.key = key;
            }
            return slot.offsets;
        });
}

const std::string& ChunkBiomeEnvironment::noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                       densityfunction::SamplerContext*) const {
    const auto localX = static_cast<std::uint32_t>(quartX - windowX_);
    const auto localY = static_cast<std::uint32_t>(quartY - windowY_);
    const auto localZ = static_cast<std::uint32_t>(quartZ - windowZ_);
    if (localX < kWindowQuarts && localZ < kWindowQuarts && localY < static_cast<std::uint32_t>(windowHeight_)) {
        std::uint8_t& slot =
            window_[(((localX * kWindowQuarts) + localZ) * static_cast<std::uint32_t>(windowHeight_)) + localY];
        if (slot != 0) {
            return *names_[slot - 1U];
        }
        const std::string& named = delegate_->noiseBiomeAt(quartX, quartY, quartZ, context_);
        for (std::size_t at = 0; at < names_.size(); ++at) {
            if (names_[at] == &named) {
                slot = static_cast<std::uint8_t>(at + 1);
                return named;
            }
        }
        if (names_.size() < 255) {
            names_.push_back(&named);
            slot = static_cast<std::uint8_t>(names_.size());
        }
        return named;
    }
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
