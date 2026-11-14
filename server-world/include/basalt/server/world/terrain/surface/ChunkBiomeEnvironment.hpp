#pragma once

#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/terrain/BiomeZoomer.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace basalt::server::world::terrain::surface {

class ChunkBiomeEnvironment final : public SurfaceRuleEnvironment {
public:
    ChunkBiomeEnvironment(const SurfaceRuleEnvironment& delegate, densityfunction::SamplerContext* context,
                          std::int32_t chunkX, std::int32_t chunkZ, ChunkHeight bounds);

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
    mutable std::unordered_map<std::int64_t, const std::string*> noiseBiomes_;

    // Dense memo for the chunk and one chunk around it; a slot holds an index into `names_` plus one, 0 when empty.
    static constexpr std::int32_t kWindowQuarts = 12;
    std::int32_t windowX_ = 0;
    std::int32_t windowY_ = 0;
    std::int32_t windowZ_ = 0;
    std::int32_t windowHeight_ = 0;
    mutable std::vector<std::uint8_t> window_;
    mutable std::vector<const std::string*> names_;

    struct CornerSlot {
        std::int64_t key = -1;
        BiomeZoomer::Offsets offsets{};
    };

    static constexpr std::size_t kCornerSlots = 512;
    mutable std::array<CornerSlot, kCornerSlots> corners_;
};

} // namespace basalt::server::world::terrain::surface
