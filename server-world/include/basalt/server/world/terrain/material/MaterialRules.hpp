#pragma once

#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/densityfunction/DensityFunctionJson.hpp>
#include <basalt/server/world/densityfunction/DensityVolume.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world {
class Chunk;
}

namespace basalt::server::world::densityfunction {
class RandomState;
class SamplerContext;
} // namespace basalt::server::world::densityfunction

namespace basalt::server::world::noise {
class Noise;
}

namespace basalt::server::world::terrain::material {

/// parity: `VerticalAnchor`.
struct VerticalAnchor {
    enum class Kind : std::uint8_t { Absolute, AboveBottom, BelowTop };
    Kind kind = Kind::Absolute;
    std::int32_t value = 0;

    [[nodiscard]] std::int32_t resolve(std::int32_t minGenY, std::int32_t genDepth) const noexcept {
        switch (kind) {
        case Kind::Absolute:
            return value;
        case Kind::AboveBottom:
            return minGenY + value;
        case Kind::BelowTop:
            break;
        }
        return minGenY + genDepth - 1 - value;
    }
};

class MaterialSystem;
class MaterialRuleContext;

using ConditionEvaluator = std::function<bool()>;
using RuleEvaluator = std::function<std::optional<std::int32_t>(std::int32_t x, std::int32_t y, std::int32_t z)>;

/// parity: `MaterialCondition`.
class MaterialCondition {
public:
    virtual ~MaterialCondition() = default;
    [[nodiscard]] virtual ConditionEvaluator compile(MaterialRuleContext& context) const = 0;
};

/// parity: `MaterialRule`.
class MaterialRule {
public:
    virtual ~MaterialRule() = default;
    [[nodiscard]] virtual RuleEvaluator compile(MaterialRuleContext& context) const = 0;
};

using BiomeGetter = std::function<const std::string&(std::int32_t x, std::int32_t y, std::int32_t z)>;

/// parity: `MaterialRuleContext`, the per-chunk state the compiled rules read.
class MaterialRuleContext {
public:
    MaterialRuleContext(MaterialSystem& system, densityfunction::RandomState& state,
                        const densityfunction::DensityVolume& expectedVolume, densityfunction::SamplerContext& samplers,
                        BiomeGetter biomes, std::int32_t minGenY, std::int32_t genDepth);

    void updateXZ(std::int32_t x, std::int32_t z, std::int32_t gradientX, std::int32_t gradientZ);
    void updateY(std::int32_t stoneDepthAbove, std::int32_t stoneDepthBelow, std::int32_t waterHeight, std::int32_t y);

    [[nodiscard]] double surfaceSecondary();
    [[nodiscard]] const std::string& biome();
    [[nodiscard]] std::int32_t minSurfaceLevel();

    [[nodiscard]] std::function<double()> noiseSampler(const std::string& noiseId, bool is3d);

    /// parity: `getDensitiesInChunk`. Prefilled over the chunk's volume when asked to.
    [[nodiscard]] std::function<float()> densities(const densityfunction::Function& function, bool prefill);

    [[nodiscard]] random::PositionalRandomFactory randomFactory(std::string_view name) const;

    [[nodiscard]] std::int32_t blockX() const noexcept { return blockX_; }

    [[nodiscard]] std::int32_t blockY() const noexcept { return blockY_; }

    [[nodiscard]] std::int32_t blockZ() const noexcept { return blockZ_; }

    [[nodiscard]] std::int32_t surfaceDepth() const noexcept { return surfaceDepth_; }

    [[nodiscard]] std::int32_t waterHeight() const noexcept { return waterHeight_; }

    [[nodiscard]] std::int32_t stoneDepthAbove() const noexcept { return stoneDepthAbove_; }

    [[nodiscard]] std::int32_t stoneDepthBelow() const noexcept { return stoneDepthBelow_; }

    [[nodiscard]] std::int32_t gradientX() const noexcept { return gradientX_; }

    [[nodiscard]] std::int32_t gradientZ() const noexcept { return gradientZ_; }

    [[nodiscard]] std::int32_t seaLevel() const noexcept;

    [[nodiscard]] std::int32_t resolve(const VerticalAnchor& anchor) const noexcept {
        return anchor.resolve(minGenY_, genDepth_);
    }

    [[nodiscard]] std::int32_t band(std::int32_t x, std::int32_t y, std::int32_t z) const;

    [[nodiscard]] std::int64_t updateXZStamp() const noexcept { return lastUpdateXZ_; }

    [[nodiscard]] std::int64_t updateYStamp() const noexcept { return lastUpdateY_; }

private:
    MaterialSystem* system_;
    densityfunction::RandomState* state_;
    densityfunction::DensityVolume expectedVolume_;
    densityfunction::SamplerContext* samplers_;
    BiomeGetter biomes_;
    std::int32_t minGenY_;
    std::int32_t genDepth_;
    densityfunction::DensityVolume preliminaryVolume_;
    std::vector<float> preliminaryBuffer_;
    bool preliminaryFilled_ = false;
    std::int64_t lastUpdateXZ_ = -9223372036854775807LL;
    std::int64_t lastUpdateY_ = -9223372036854775807LL;
    std::int64_t lastSecondaryUpdate_;
    std::int64_t lastMinSurfaceUpdate_;
    std::int32_t blockX_ = 0;
    std::int32_t blockY_ = 0;
    std::int32_t blockZ_ = 0;
    std::int32_t gradientX_ = 0;
    std::int32_t gradientZ_ = 0;
    std::int32_t surfaceDepth_ = 0;
    double surfaceSecondary_ = 0.0;
    std::int32_t minSurfaceLevel_ = 0;
    const std::string* biome_ = nullptr;
    std::int32_t waterHeight_ = 0;
    std::int32_t stoneDepthBelow_ = 0;
    std::int32_t stoneDepthAbove_ = 0;
};

/// parity: `MaterialSystem`.
class MaterialSystem {
public:
    MaterialSystem(densityfunction::RandomState& state, std::int32_t defaultBlock, std::int32_t seaLevel,
                   densityfunction::Function chunkSurfaceLevel);

    /// parity: `MaterialSystem.buildSurface`. `biomes` reads the zoomed biome at a block.
    void buildSurface(Chunk& chunk, densityfunction::SamplerContext& samplers,
                      const densityfunction::DensityVolume& volume, const MaterialRule& rule, const BiomeGetter& biomes,
                      std::int32_t minGenY, std::int32_t genDepth);

    /// parity: `MaterialSystem.topMaterial`, what a carver puts back on exposed dirt.
    [[nodiscard]] std::optional<std::int32_t> topMaterial(const MaterialRule& rule, const Chunk& chunk,
                                                          densityfunction::SamplerContext& samplers,
                                                          const BiomeGetter& biomes, std::int32_t x, std::int32_t y,
                                                          std::int32_t z, bool underFluid, std::int32_t minGenY,
                                                          std::int32_t genDepth);

    [[nodiscard]] std::int32_t surfaceDepth(std::int32_t x, std::int32_t z) const;
    [[nodiscard]] double surfaceSecondary(std::int32_t x, std::int32_t z) const;
    [[nodiscard]] std::int32_t band(std::int32_t x, std::int32_t y, std::int32_t z) const;

    [[nodiscard]] std::int32_t seaLevel() const noexcept { return seaLevel_; }

    [[nodiscard]] const densityfunction::Function& chunkSurfaceLevel() const noexcept { return chunkSurfaceLevel_; }

    [[nodiscard]] densityfunction::RandomState& state() const noexcept { return *state_; }

private:
    void erodedBadlandsExtension(Chunk& chunk, std::int32_t localX, std::int32_t localZ, std::int32_t blockX,
                                 std::int32_t blockZ, std::int32_t height) const;
    void frozenOceanExtension(Chunk& chunk, std::int32_t minSurfaceLevel, const std::string& biome, std::int32_t localX,
                              std::int32_t localZ, std::int32_t blockX, std::int32_t blockZ, std::int32_t height) const;

    densityfunction::RandomState* state_;
    std::int32_t defaultBlock_;
    std::int32_t seaLevel_;
    densityfunction::Function chunkSurfaceLevel_;
    std::array<std::int32_t, 192> clayBands_{};
    std::shared_ptr<const noise::Noise> clayBandsOffset_;
    std::shared_ptr<const noise::Noise> surface_;
    std::shared_ptr<const noise::Noise> surfaceSecondary_;
    std::shared_ptr<const noise::Noise> badlandsPillar_;
    std::shared_ptr<const noise::Noise> badlandsPillarRoof_;
    std::shared_ptr<const noise::Noise> badlandsSurface_;
    std::shared_ptr<const noise::Noise> icebergPillar_;
    std::shared_ptr<const noise::Noise> icebergPillarRoof_;
    std::shared_ptr<const noise::Noise> icebergSurface_;
};

class MaterialRuleLoader {
public:
    explicit MaterialRuleLoader(densityfunction::DensityFunctionLoader& functions);

    [[nodiscard]] std::expected<std::shared_ptr<const MaterialRule>, std::string>
    rule(const wire::common::JsonValue& json);

    [[nodiscard]] std::expected<std::shared_ptr<const MaterialCondition>, std::string>
    condition(const wire::common::JsonValue& json);

private:
    densityfunction::DensityFunctionLoader* functions_;
    std::map<std::string, std::shared_ptr<const MaterialRule>, std::less<>> rules_;
    std::map<std::string, std::shared_ptr<const MaterialCondition>, std::less<>> conditions_;
};

} // namespace basalt::server::world::terrain::material
