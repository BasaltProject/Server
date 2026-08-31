#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/Gradients.hpp>

#include <array>
#include <cstdint>

namespace basalt::server::world::noise {

/// parity: net.minecraft.world.level.levelgen.synth.SimplexNoise. The standalone 2D and 3D simplex
/// value noise vanilla still exposes for a handful of callers.
class SimplexNoise {
public:
    explicit SimplexNoise(random::RandomSource& source);

    [[nodiscard]] double xo() const noexcept { return xo_; }

    [[nodiscard]] double yo() const noexcept { return yo_; }

    [[nodiscard]] double zo() const noexcept { return zo_; }

    [[nodiscard]] double getValue(double xin, double yin) const noexcept;

    [[nodiscard]] double getValue(double xin, double yin, double zin) const noexcept;

    [[nodiscard]] static double dot(const std::array<std::int32_t, 3>& g, double x, double y, double z) noexcept {
        return g[0] * x + g[1] * y + g[2] * z;
    }

private:
    [[nodiscard]] std::int32_t p(std::int32_t x) const noexcept {
        return permutation_[static_cast<std::uint32_t>(x) & 0xFFU];
    }

    [[nodiscard]] static double cornerNoise3d(std::int32_t index, double x, double y, double z, double base) noexcept;

    std::array<std::int32_t, 512> permutation_{};
    double xo_ = 0.0;
    double yo_ = 0.0;
    double zo_ = 0.0;
};

} // namespace basalt::server::world::noise
