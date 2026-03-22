#pragma once

#include <basalt/server/world/terrain/BiomeZoomer.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <optional>
#include <string>
#include <unordered_map>

namespace basalt::server::world::terrain::surface {

/// Caches raw quart biomes while exposing BiomeManager's zoomed block-position lookup.
class ChunkBiomeEnvironment final : public SurfaceRuleEnvironment {
public:
    ChunkBiomeEnvironment(const SurfaceRuleEnvironment& delegate, densityfunction::SamplerContext* context);

    [[nodiscard]] std::optional<std::int64_t> biomeZoomSeed() const override { return delegate_->biomeZoomSeed(); }

    [[nodiscard]] const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const override;

    [[nodiscard]] const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                             densityfunction::SamplerContext*) const override {
        return biomeAt(x, y, z);
    }

    [[nodiscard]] const std::string& noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                  densityfunction::SamplerContext* context = nullptr) const override;

private:
    const SurfaceRuleEnvironment* delegate_;
    densityfunction::SamplerContext* context_;
    std::optional<BiomeZoomer> zoomer_;
    // Mutable because the lookups are logically const: the cache is memoization, not state.
    mutable std::unordered_map<std::int64_t, std::string> noiseBiomes_;
};

} // namespace basalt::server::world::terrain::surface
