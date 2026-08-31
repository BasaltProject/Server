#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/Gradients.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <array>
#include <cstdint>

namespace basalt::server::world::noise {

/// parity: net.minecraft.world.level.levelgen.synth.ImprovedNoise, classic Perlin noise and the
/// per-octave building block of PerlinNoise. `noiseWithDerivative` (density-function gradient
/// sampling) is not implemented: nothing in this project calls it.
///
/// Defined inline because PerlinNoise's octave loop is the hottest loop in world generation and
/// wants this body in front of it.
class ImprovedNoise {
public:
    explicit ImprovedNoise(random::RandomSource& source);

    [[nodiscard]] double xo() const noexcept { return xo_; }

    [[nodiscard]] double yo() const noexcept { return yo_; }

    [[nodiscard]] double zo() const noexcept { return zo_; }

    [[nodiscard]] double noise(double x, double y, double z) const noexcept { return noise(x, y, z, 0.0, 0.0); }

    [[nodiscard]] double noise(double x, double y, double z, double yScale, double yFudge) const noexcept {
        namespace mth = wire::common::mth;
        const double xx = x + xo_;
        const double yy = y + yo_;
        const double zz = z + zo_;
        const std::int32_t xf = mth::floor(xx);
        const std::int32_t yf = mth::floor(yy);
        const std::int32_t zf = mth::floor(zz);
        const double xr = xx - xf;
        const double yr = yy - yf;
        const double zr = zz - zf;

        double yrFudge = 0.0;
        if (yScale != 0.0) {
            const double fudgeLimit = (yFudge >= 0.0 && yFudge < yr) ? yFudge : yr;
            yrFudge = mth::floor(fudgeLimit / yScale + kShiftUpEpsilon) * yScale;
        }
        return sampleAndLerp(xf, yf, zf, xr, yr - yrFudge, zr, yr);
    }

private:
    static constexpr double kShiftUpEpsilon = static_cast<double>(1.0E-7F);

    [[nodiscard]] std::int32_t p(std::int32_t x) const noexcept {
        return permutation_[static_cast<std::uint32_t>(x) & 0xFFU];
    }

    [[nodiscard]] static double gradDot(std::int32_t hash, double x, double y, double z) noexcept {
        const auto base = static_cast<std::size_t>(hash & 15) * 3;
        return kGradients[base] * x + kGradients[base + 1] * y + kGradients[base + 2] * z;
    }

    [[nodiscard]] double sampleAndLerp(std::int32_t x, std::int32_t y, std::int32_t z, double xr, double yr, double zr,
                                       double yrOriginal) const noexcept {
        namespace mth = wire::common::mth;
        const std::int32_t x0 = p(x);
        const std::int32_t x1 = p(x + 1);
        const std::int32_t xy00 = p(x0 + y);
        const std::int32_t xy01 = p(x0 + y + 1);
        const std::int32_t xy10 = p(x1 + y);
        const std::int32_t xy11 = p(x1 + y + 1);
        const double d000 = gradDot(p(xy00 + z), xr, yr, zr);
        const double d100 = gradDot(p(xy10 + z), xr - 1.0, yr, zr);
        const double d010 = gradDot(p(xy01 + z), xr, yr - 1.0, zr);
        const double d110 = gradDot(p(xy11 + z), xr - 1.0, yr - 1.0, zr);
        const double d001 = gradDot(p(xy00 + z + 1), xr, yr, zr - 1.0);
        const double d101 = gradDot(p(xy10 + z + 1), xr - 1.0, yr, zr - 1.0);
        const double d011 = gradDot(p(xy01 + z + 1), xr, yr - 1.0, zr - 1.0);
        const double d111 = gradDot(p(xy11 + z + 1), xr - 1.0, yr - 1.0, zr - 1.0);
        const double xAlpha = mth::smoothstep(xr);
        const double yAlpha = mth::smoothstep(yrOriginal);
        const double zAlpha = mth::smoothstep(zr);
        return mth::lerp3(xAlpha, yAlpha, zAlpha, d000, d100, d010, d110, d001, d101, d011, d111);
    }

    std::array<std::uint8_t, 256> permutation_{};
    double xo_ = 0.0;
    double yo_ = 0.0;
    double zo_ = 0.0;
};

} // namespace basalt::server::world::noise
