#pragma once

#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/densityfunction/DensityCache.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace basalt::server::world::terrain {

/// parity: `Aquifer.NoiseBasedAquifer` for the 26.2 Overworld settings. One instance belongs to one
/// chunk because vanilla's aquifer-center and fluid-status arrays are chunk-local caches.
class OverworldAquifer {
public:
    OverworldAquifer(const OverworldNoiseRouter& router, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                     std::int32_t minY, std::int32_t height, densityfunction::DensityCache* densityCache = nullptr);

    /// Null for stone, otherwise the exact air, water or lava material vanilla selects.
    [[nodiscard]] const registry::BlockState* computeSubstance(std::int32_t x, std::int32_t y, std::int32_t z,
                                                               double density);

    [[nodiscard]] bool shouldScheduleFluidUpdate() const noexcept { return shouldScheduleFluidUpdate_; }

private:
    struct AquiferPosition {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
    };

    struct FluidStatus {
        std::int32_t fluidLevel = 0;
        const registry::BlockState* fluidType = nullptr;

        [[nodiscard]] const registry::BlockState* at(std::int32_t y) const;

        friend bool operator==(const FluidStatus&, const FluidStatus&) = default;
    };

    /// parity: `Aquifer.NoiseBasedAquifer.getGlobalFluidPicker`, the sea above and the lava sea below.
    [[nodiscard]] static FluidStatus globalFluid(std::int32_t y);

    [[nodiscard]] double calculatePressure(densityfunction::FunctionContext& context,
                                           std::optional<double>& barrierNoiseValue, const FluidStatus& first,
                                           const FluidStatus& second) const;
    [[nodiscard]] const FluidStatus& status(std::int32_t index);
    [[nodiscard]] FluidStatus computeFluid(std::int32_t x, std::int32_t y, std::int32_t z);
    [[nodiscard]] std::int32_t computeSurfaceLevel(std::int32_t x, std::int32_t y, std::int32_t z,
                                                   const FluidStatus& globalFluid,
                                                   std::int32_t lowestPreliminarySurface,
                                                   bool surfaceAtCenterIsUnderGlobalFluid) const;
    [[nodiscard]] std::int32_t randomizedFluidSurfaceLevel(std::int32_t x, std::int32_t y, std::int32_t z,
                                                           std::int32_t lowestPreliminarySurface) const;
    [[nodiscard]] const registry::BlockState* computeFluidType(std::int32_t x, std::int32_t y, std::int32_t z,
                                                               const FluidStatus& globalFluid,
                                                               std::int32_t fluidSurface) const;
    [[nodiscard]] std::int32_t preliminarySurfaceLevel(std::int32_t x, std::int32_t z);
    [[nodiscard]] std::int32_t maxPreliminarySurfaceLevel(std::int32_t minX, std::int32_t minZ, std::int32_t maxX,
                                                          std::int32_t maxZ);
    [[nodiscard]] std::int32_t index(std::int32_t gridX, std::int32_t gridY, std::int32_t gridZ) const;

    const OverworldNoiseRouter& router_;
    densityfunction::DensityCache* densityCache_;
    random::PositionalRandomFactory positionalRandomFactory_;
    std::unordered_map<std::int64_t, std::int32_t> preliminarySurfaceCache_;
    std::int32_t minGridX_;
    std::int32_t minGridY_;
    std::int32_t minGridZ_;
    std::int32_t gridSizeX_;
    std::int32_t gridSizeZ_;
    std::vector<std::optional<AquiferPosition>> locationCache_;
    std::vector<std::optional<FluidStatus>> statusCache_;
    std::int32_t skipSamplingAboveY_ = 0;
    bool shouldScheduleFluidUpdate_ = false;
};

} // namespace basalt::server::world::terrain
