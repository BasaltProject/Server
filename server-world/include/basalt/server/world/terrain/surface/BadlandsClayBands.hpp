#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <array>
#include <cstdint>

namespace basalt::server::world::terrain::surface {

/// parity: `SurfaceSystem.generateBands`, the per-world 192-block badlands clay palette.
class BadlandsClayBands {
public:
    explicit BadlandsClayBands(random::RandomSource& source);

    [[nodiscard]] std::int32_t stateId(std::int32_t y, double offsetNoise) const;

    [[nodiscard]] const std::array<std::int32_t, 192>& stateIds() const noexcept { return bands_; }

private:
    std::array<std::int32_t, 192> bands_{};
};

} // namespace basalt::server::world::terrain::surface
