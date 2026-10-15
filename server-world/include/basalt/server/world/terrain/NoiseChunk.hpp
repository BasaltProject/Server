#pragma once

#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/world/densityfunction/SamplerContext.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace basalt::server::world {
class Chunk;
}

namespace basalt::server::world::terrain {

/// parity: `Aquifer`.
class Aquifer {
public:
    Aquifer(const NoiseRouter& router, densityfunction::SamplerContext& samplers,
            const densityfunction::DensityVolume& volume);

    [[nodiscard]] std::optional<std::int32_t> computeSubstance(std::int32_t x, std::int32_t y, std::int32_t z,
                                                               double density);

    [[nodiscard]] bool shouldScheduleFluidUpdate() const noexcept { return shouldScheduleFluidUpdate_; }

private:
    struct FluidStatus {
        std::int32_t fluidLevel = 0;
        std::int32_t fluidType = 0;

        [[nodiscard]] std::int32_t at(std::int32_t y) const noexcept;

        friend bool operator==(const FluidStatus&, const FluidStatus&) = default;
    };

    [[nodiscard]] FluidStatus globalFluid(std::int32_t y) const noexcept;
    [[nodiscard]] double calculatePressure(std::int32_t x, std::int32_t y, std::int32_t z,
                                           std::optional<double>& barrier, const FluidStatus& first,
                                           const FluidStatus& second);
    [[nodiscard]] const FluidStatus& status(std::int32_t index);
    [[nodiscard]] FluidStatus computeFluid(std::int32_t x, std::int32_t y, std::int32_t z);
    [[nodiscard]] std::int32_t computeSurfaceLevel(std::int32_t x, std::int32_t y, std::int32_t z,
                                                   const FluidStatus& global, std::int32_t lowestSurface,
                                                   bool centerUnderGlobalFluid);
    [[nodiscard]] std::int32_t computeFluidType(std::int32_t x, std::int32_t y, std::int32_t z,
                                                const FluidStatus& global, std::int32_t fluidSurface);
    [[nodiscard]] std::int32_t surfaceLevel(std::int32_t x, std::int32_t z);
    [[nodiscard]] std::int32_t maxSurfaceLevel(std::int32_t minX, std::int32_t minZ, std::int32_t maxX,
                                               std::int32_t maxZ);
    [[nodiscard]] std::int32_t index(std::int32_t gridX, std::int32_t gridY, std::int32_t gridZ) const noexcept;
    [[nodiscard]] float sample(const densityfunction::Sampler& sampler, std::int32_t x, std::int32_t y,
                               std::int32_t z) const;

    const NoiseRouter* router_;
    densityfunction::SamplerContext* samplers_;
    bool enabled_;
    densityfunction::Sampler barrier_;
    densityfunction::Sampler floodedness_;
    densityfunction::Sampler spread_;
    densityfunction::Sampler lava_;
    densityfunction::Sampler exclusion_;
    densityfunction::Sampler surface_;
    std::optional<random::PositionalRandomFactory> random_;
    std::unordered_map<std::int64_t, std::int32_t> surfaceCache_;
    std::int32_t minGridX_ = 0;
    std::int32_t minGridY_ = 0;
    std::int32_t minGridZ_ = 0;
    std::int32_t gridSizeX_ = 0;
    std::int32_t gridSizeZ_ = 0;
    std::vector<std::int64_t> locations_;
    std::vector<std::optional<FluidStatus>> statuses_;
    std::int32_t skipSamplingAboveY_ = 0;
    bool shouldScheduleFluidUpdate_ = false;
};

/// parity: `NoiseChunk`.
class NoiseChunk {
public:
    NoiseChunk(const NoiseRouter& router, densityfunction::Sampler beardifier,
               const densityfunction::DensityVolume& volume);

    [[nodiscard]] densityfunction::SamplerContext& samplers() noexcept { return samplers_; }

    [[nodiscard]] Aquifer& aquifer() noexcept { return aquifer_; }

    [[nodiscard]] const densityfunction::DensityVolume& volume() const noexcept { return volume_; }

    [[nodiscard]] std::vector<float> finalDensity();

private:
    const NoiseRouter* router_;
    densityfunction::DensityVolume volume_;
    densityfunction::SamplerContext samplers_;
    Aquifer aquifer_;
};

/// parity: `NoiseBasedChunkGenerator.doFill`.
void fillNoise(Chunk& chunk, NoiseChunk& noiseChunk, const NoiseRouter& router);

} // namespace basalt::server::world::terrain
