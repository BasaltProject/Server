#include <basalt/server/world/noise/SimplexNoise.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>

namespace basalt::server::world::noise {
namespace {

const double kSqrt3 = std::sqrt(3.0);
const double kF2 = 0.5 * (kSqrt3 - 1.0);
const double kG2 = (3.0 - kSqrt3) / 6.0;
constexpr double kF3 = 1.0 / 3.0;
constexpr double kG3 = 1.0 / 6.0;

} // namespace

SimplexNoise::SimplexNoise(random::RandomSource& source) {
    xo_ = source.nextDouble() * 256.0;
    yo_ = source.nextDouble() * 256.0;
    zo_ = source.nextDouble() * 256.0;

    for (std::int32_t i = 0; i < 256; ++i) {
        permutation_[static_cast<std::size_t>(i)] = i;
    }
    for (std::int32_t i = 0; i < 256; ++i) {
        const std::int32_t offset = source.nextInt(256 - i);
        const auto at = static_cast<std::size_t>(i);
        const auto swapWith = static_cast<std::size_t>(offset + i);
        const std::int32_t tmp = permutation_[at];
        permutation_[at] = permutation_[swapWith];
        permutation_[swapWith] = tmp;
    }
}

double SimplexNoise::cornerNoise3d(std::int32_t index, double x, double y, double z, double base) noexcept {
    double t0 = base - x * x - y * y - z * z;
    if (t0 < 0.0) {
        return 0.0;
    }
    t0 *= t0;
    return t0 * t0 * dot(kGradientRows[static_cast<std::size_t>(index)], x, y, z);
}

double SimplexNoise::getValue(double xin, double yin) const noexcept {
    namespace mth = wire::common::mth;
    const double s = (xin + yin) * kF2;
    const std::int32_t i = mth::floor(xin + s);
    const std::int32_t j = mth::floor(yin + s);
    const double t = (i + j) * kG2;
    const double x0 = xin - (i - t);
    const double y0 = yin - (j - t);
    const std::int32_t i1 = x0 > y0 ? 1 : 0;
    const std::int32_t j1 = x0 > y0 ? 0 : 1;
    const double x1 = x0 - i1 + kG2;
    const double y1 = y0 - j1 + kG2;
    const double x2 = x0 - 1.0 + 2.0 * kG2;
    const double y2 = y0 - 1.0 + 2.0 * kG2;
    const std::int32_t ii = i & 0xFF;
    const std::int32_t jj = j & 0xFF;
    const std::int32_t gi0 = p(ii + p(jj)) % 12;
    const std::int32_t gi1 = p(ii + i1 + p(jj + j1)) % 12;
    const std::int32_t gi2 = p(ii + 1 + p(jj + 1)) % 12;
    const double n0 = cornerNoise3d(gi0, x0, y0, 0.0, 0.5);
    const double n1 = cornerNoise3d(gi1, x1, y1, 0.0, 0.5);
    const double n2 = cornerNoise3d(gi2, x2, y2, 0.0, 0.5);
    return 70.0 * (n0 + n1 + n2);
}

double SimplexNoise::getValue(double xin, double yin, double zin) const noexcept {
    namespace mth = wire::common::mth;
    const double s = (xin + yin + zin) * kF3;
    const std::int32_t i = mth::floor(xin + s);
    const std::int32_t j = mth::floor(yin + s);
    const std::int32_t k = mth::floor(zin + s);
    const double t = (i + j + k) * kG3;
    const double x0 = xin - (i - t);
    const double y0 = yin - (j - t);
    const double z0 = zin - (k - t);

    std::int32_t i1 = 0;
    std::int32_t j1 = 0;
    std::int32_t k1 = 0;
    std::int32_t i2 = 0;
    std::int32_t j2 = 0;
    std::int32_t k2 = 0;
    if (x0 >= y0) {
        if (y0 >= z0) {
            i1 = 1;
            i2 = 1;
            j2 = 1;
        } else if (x0 >= z0) {
            i1 = 1;
            i2 = 1;
            k2 = 1;
        } else {
            k1 = 1;
            i2 = 1;
            k2 = 1;
        }
    } else {
        if (y0 < z0) {
            k1 = 1;
            j2 = 1;
            k2 = 1;
        } else if (x0 < z0) {
            j1 = 1;
            j2 = 1;
            k2 = 1;
        } else {
            j1 = 1;
            i2 = 1;
            j2 = 1;
        }
    }

    const double x1 = x0 - i1 + kG3;
    const double y1 = y0 - j1 + kG3;
    const double z1 = z0 - k1 + kG3;
    const double x2 = x0 - i2 + 2.0 * kG3;
    const double y2 = y0 - j2 + 2.0 * kG3;
    const double z2 = z0 - k2 + 2.0 * kG3;
    const double x3 = x0 - 1.0 + 3.0 * kG3;
    const double y3 = y0 - 1.0 + 3.0 * kG3;
    const double z3 = z0 - 1.0 + 3.0 * kG3;
    const std::int32_t ii = i & 0xFF;
    const std::int32_t jj = j & 0xFF;
    const std::int32_t kk = k & 0xFF;
    const std::int32_t gi0 = p(ii + p(jj + p(kk))) % 12;
    const std::int32_t gi1 = p(ii + i1 + p(jj + j1 + p(kk + k1))) % 12;
    const std::int32_t gi2 = p(ii + i2 + p(jj + j2 + p(kk + k2))) % 12;
    const std::int32_t gi3 = p(ii + 1 + p(jj + 1 + p(kk + 1))) % 12;
    const double n0 = cornerNoise3d(gi0, x0, y0, z0, 0.6);
    const double n1 = cornerNoise3d(gi1, x1, y1, z1, 0.6);
    const double n2 = cornerNoise3d(gi2, x2, y2, z2, 0.6);
    const double n3 = cornerNoise3d(gi3, x3, y3, z3, 0.6);
    return 32.0 * (n0 + n1 + n2 + n3);
}

} // namespace basalt::server::world::noise
