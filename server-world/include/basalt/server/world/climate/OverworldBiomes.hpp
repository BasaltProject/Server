#pragma once

#include <basalt/server/world/climate/Climate.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::world::climate {

/// Where a biome sits, as its own name. The registry ids are dynamic and synced per world, so the
/// list is built in names and resolved by whoever needs numbers.
using BiomeName = std::string_view;

/// parity: `OverworldBiomeBuilder`, ported mechanically: every threshold, table and branch is
/// vanilla's, not an approximation.
///
/// Biomes are regions in the six-axis climate space and the closest one wins, ties broken by
/// first-registered. The call order is therefore semantically significant and must never be
/// reordered; the test that guards this asserts the whole emitted list, in order, against the
/// frozen Kotlin's.
namespace overworldBiomes {

using Emit = std::function<void(const ParameterPoint&, BiomeName)>;

/// Every parameter point of the overworld, in the order vanilla registers them.
[[nodiscard]] std::vector<std::pair<ParameterPoint, BiomeName>> build();

/// The static spans and lookup tables `build` walks, plus the pickers that only read them.
class Tables {
public:
    Tables();

    [[nodiscard]] BiomeName pickMiddleBiome(std::size_t t, std::size_t h, const Parameter& weirdness) const;
    [[nodiscard]] BiomeName pickMiddleBiomeOrBadlandsIfHot(std::size_t t, std::size_t h,
                                                           const Parameter& weirdness) const;
    [[nodiscard]] BiomeName pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(std::size_t t, std::size_t h,
                                                                        const Parameter& weirdness) const;
    [[nodiscard]] BiomeName maybePickWindsweptSavannaBiome(std::size_t t, std::size_t h, const Parameter& weirdness,
                                                           BiomeName underlying) const;
    [[nodiscard]] BiomeName pickShatteredCoastBiome(std::size_t t, std::size_t h, const Parameter& weirdness) const;
    [[nodiscard]] static BiomeName pickBeachBiome(std::size_t t, std::size_t h);
    [[nodiscard]] static BiomeName pickBadlandsBiome(std::size_t h, const Parameter& weirdness);
    [[nodiscard]] BiomeName pickPlateauBiome(std::size_t t, std::size_t h, const Parameter& weirdness) const;
    [[nodiscard]] BiomeName pickPeakBiome(std::size_t t, std::size_t h, const Parameter& weirdness) const;
    [[nodiscard]] BiomeName pickSlopeBiome(std::size_t t, std::size_t h, const Parameter& weirdness) const;
    [[nodiscard]] BiomeName pickShatteredBiome(std::size_t t, std::size_t h, const Parameter& weirdness) const;

    Parameter fullRange;
    std::array<Parameter, 5> temperatures;
    std::array<Parameter, 5> humidities;
    std::array<Parameter, 7> erosions;
    Parameter frozenRange;
    Parameter unfrozenRange;
    Parameter mushroomFieldsContinentalness;
    Parameter deepOceanContinentalness;
    Parameter oceanContinentalness;
    Parameter coastContinentalness;
    Parameter inlandContinentalness;
    Parameter nearInlandContinentalness;
    Parameter midInlandContinentalness;
    Parameter farInlandContinentalness;

    std::array<std::array<BiomeName, 5>, 2> oceans;
    std::array<std::array<BiomeName, 5>, 5> middleBiomes;
    std::array<std::array<std::optional<BiomeName>, 5>, 5> middleBiomesVariant;
    std::array<std::array<BiomeName, 5>, 5> plateauBiomes;
    std::array<std::array<std::optional<BiomeName>, 5>, 5> plateauBiomesVariant;
    std::array<std::array<std::optional<BiomeName>, 5>, 5> shatteredBiomes;

    /// The three shapes vanilla emits a pair in: a surface pair spanning depth zero and one, an
    /// underground span, and a single bottom point.
    static void addSurfaceBiome(const Emit& emit, const Parameter& temperature, const Parameter& humidity,
                                const Parameter& continentalness, const Parameter& erosion, const Parameter& weirdness,
                                float offset, BiomeName biome);
    static void addUndergroundBiome(const Emit& emit, const Parameter& temperature, const Parameter& humidity,
                                    const Parameter& continentalness, const Parameter& erosion,
                                    const Parameter& weirdness, float offset, BiomeName biome);
    static void addBottomBiome(const Emit& emit, const Parameter& temperature, const Parameter& humidity,
                               const Parameter& continentalness, const Parameter& erosion, const Parameter& weirdness,
                               float offset, BiomeName biome);

    void addOffCoastBiomes(const Emit& emit) const;
    void addUndergroundBiomes(const Emit& emit) const;
    void addMidSlice(const Emit& emit, const Parameter& weirdness) const;
    void addHighSlice(const Emit& emit, const Parameter& weirdness) const;
    void addPeaks(const Emit& emit, const Parameter& weirdness) const;
    void addLowSlice(const Emit& emit, const Parameter& weirdness) const;
    void addValleys(const Emit& emit, const Parameter& weirdness) const;
};

} // namespace overworldBiomes

} // namespace basalt::server::world::climate
