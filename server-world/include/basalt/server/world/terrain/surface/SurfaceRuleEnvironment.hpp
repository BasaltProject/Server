#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace basalt::server::world::densityfunction {
class SamplerContext;
}

namespace basalt::server::world::terrain::surface {

class SurfaceRuleEnvironment {
public:
    SurfaceRuleEnvironment() = default;
    SurfaceRuleEnvironment(const SurfaceRuleEnvironment&) = default;
    SurfaceRuleEnvironment(SurfaceRuleEnvironment&&) = default;
    SurfaceRuleEnvironment& operator=(const SurfaceRuleEnvironment&) = default;
    SurfaceRuleEnvironment& operator=(SurfaceRuleEnvironment&&) = default;
    virtual ~SurfaceRuleEnvironment() = default;

    [[nodiscard]] virtual std::optional<std::int64_t> biomeZoomSeed() const { return std::nullopt; }

    [[nodiscard]] virtual const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const = 0;

    [[nodiscard]] virtual const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                                     densityfunction::SamplerContext* context) const {
        (void)context;
        return biomeAt(x, y, z);
    }

    [[nodiscard]] virtual const std::string& noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                          densityfunction::SamplerContext* context = nullptr) const = 0;
};

} // namespace basalt::server::world::terrain::surface
