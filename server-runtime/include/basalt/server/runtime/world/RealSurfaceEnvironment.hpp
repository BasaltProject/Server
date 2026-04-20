#pragma once

#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/OverworldBiomeSource.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

class RealSurfaceEnvironment final : public server::world::terrain::surface::SurfaceRuleEnvironment {
public:
    /// Borrowed, not copied: shared by every generation worker and must outlive this.
    RealSurfaceEnvironment(const server::world::terrain::NoiseRouter& router, std::int64_t biomeZoomSeed);

    [[nodiscard]] std::optional<std::int64_t> biomeZoomSeed() const override { return biomeZoomSeed_; }

    [[nodiscard]] const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const override;

    [[nodiscard]] const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                             server::world::densityfunction::SamplerContext* context) const override;

    [[nodiscard]] const std::string&
    noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                 server::world::densityfunction::SamplerContext* context = nullptr) const override;

private:
    [[nodiscard]] const std::string& nameOf(std::string_view biome) const;

    std::int64_t biomeZoomSeed_;
    server::world::terrain::OverworldBiomeSource biomeSource_;
    mutable std::mutex mutex_;
    mutable std::map<std::string, std::string, std::less<>> biomeNames_;
};

} // namespace basalt::server::runtime::world
