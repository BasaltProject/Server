#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/JavaMath.hpp>
#include <basalt/server/world/noise/Noise.hpp>

#include <cmath>
#include <limits>
#include <utility>

namespace basalt::server::world::noise {
namespace {

namespace jm = javaMath;

using densityfunction::DensityBuffer;
using densityfunction::DensityVolume;

constexpr double kRoundOff = 33554432.0;

/// parity: `Math.nextDown((double) 1.6777216E7F)`.
const double kHalfRoundOff = std::nextafter(16777216.0, 0.0);

/// The eight corner dot products split into their xz part and their y gradient, which the volume
/// path computes once per y cell and reuses while y stays inside it.
struct CornerCache {
    std::array<float, 8> xz{};
    std::array<float, 8> gy{};
};

void fillCorners(const GradientNoise& noise, CornerCache& corners, std::int32_t x0, std::int32_t x1,
                 std::int32_t floorY, std::int32_t floorZ, float relativeX, float relativeZ) {
    const std::int32_t xy00 = noise.permute(x0 + floorY);
    const std::int32_t xy01 = noise.permute(x0 + floorY + 1);
    const std::int32_t xy10 = noise.permute(x1 + floorY);
    const std::int32_t xy11 = noise.permute(x1 + floorY + 1);
    const auto grad = [&noise](std::int32_t hash) -> const GradientNoise::Gradient& {
        return GradientNoise::kGradients[static_cast<std::size_t>(noise.permute(hash) & 15)];
    };
    // Order: 000, 100, 010, 110, 001, 101, 011, 111.
    const std::array<std::int32_t, 8> hashes{xy00 + floorZ,     xy10 + floorZ,     xy01 + floorZ,
                                             xy11 + floorZ,     xy00 + floorZ + 1, xy10 + floorZ + 1,
                                             xy01 + floorZ + 1, xy11 + floorZ + 1};
    for (std::size_t i = 0; i < 8; ++i) {
        const GradientNoise::Gradient& g = grad(hashes[i]);
        const float dx = (i & 1U) != 0 ? relativeX - 1.0F : relativeX;
        const float dz = (i & 4U) != 0 ? relativeZ - 1.0F : relativeZ;
        corners.xz[i] = g.dotXz(dx, dz);
        corners.gy[i] = static_cast<float>(g.y);
    }
}

[[nodiscard]] float lerpCorners(const CornerCache& c, float alphaX, float alphaY, float alphaZ, float relativeY) {
    const float below = relativeY;
    const float above = relativeY - 1.0F;
    return jm::lerp3(alphaX, alphaY, alphaZ, c.xz[0] + c.gy[0] * below, c.xz[1] + c.gy[1] * below,
                     c.xz[2] + c.gy[2] * above, c.xz[3] + c.gy[3] * above, c.xz[4] + c.gy[4] * below,
                     c.xz[5] + c.gy[5] * below, c.xz[6] + c.gy[6] * above, c.xz[7] + c.gy[7] * above);
}

[[nodiscard]] double cornerNoise(std::int32_t index, double x, double y, double z, double base) {
    double t0 = base - x * x - y * y - z * z;
    if (t0 < 0.0) {
        return 0.0;
    }
    t0 *= t0;
    return t0 * t0 * GradientNoise::kGradients[static_cast<std::size_t>(index)].dot(x, y, z);
}

} // namespace

void Noise::addToVolume(DensityBuffer& buffer, const DensityVolume& volume, double xzScale, double yScale,
                        float amplitude) const {
    std::int32_t index = 0;
    for (std::int32_t z = 0; z < volume.sizeZ; ++z) {
        const double sampleZ = static_cast<double>(volume.blockZ(z)) * xzScale;
        for (std::int32_t x = 0; x < volume.sizeX; ++x) {
            const double sampleX = static_cast<double>(volume.blockX(x)) * xzScale;
            for (std::int32_t y = 0; y < volume.sizeY; ++y) {
                const double sampleY = static_cast<double>(volume.blockY(y)) * yScale;
                buffer.addTo(index, amplitude * get(sampleX, sampleY, sampleZ));
                ++index;
            }
        }
    }
}

GradientNoise::GradientNoise(random::RandomSource& random, double noiseOffsetScale) {
    offsetX_ = random.nextDouble() * noiseOffsetScale;
    offsetY_ = random.nextDouble() * noiseOffsetScale;
    offsetZ_ = random.nextDouble() * noiseOffsetScale;
    for (std::size_t i = 0; i < 256; ++i) {
        perms_[i] = static_cast<std::uint8_t>(i);
    }
    for (std::int32_t i = 0; i < 256; ++i) {
        const std::int32_t offset = random.nextInt(256 - i);
        std::swap(perms_[static_cast<std::size_t>(i)], perms_[static_cast<std::size_t>(offset + i)]);
    }
}

double GradientNoise::wrap(double x) noexcept {
    if (x >= -kHalfRoundOff && x < kHalfRoundOff) {
        return x;
    }
    return x - std::floor(x / kRoundOff + 0.5) * kRoundOff;
}

float PerlinNoise::get(double x, double y) const { return get(wrap(x), 0.0, wrap(y)); }

float PerlinNoise::get(double inX, double inY, double inZ) const {
    const double x = wrap(inX) + offsetX();
    const double y = wrap(inY) + offsetY();
    const double z = wrap(inZ) + offsetZ();
    const std::int32_t floorX = jm::floor(x);
    const std::int32_t floorY = jm::floor(y);
    const std::int32_t floorZ = jm::floor(z);
    const auto relativeX = static_cast<float>(x - floorX);
    const auto relativeY = static_cast<float>(y - floorY);
    const auto relativeZ = static_cast<float>(z - floorZ);
    return sampleAndLerp(floorX, floorY, floorZ, relativeX, relativeY, relativeZ, relativeY);
}

float PerlinNoise::sampleAndLerp(std::int32_t x, std::int32_t y, std::int32_t z, float relativeX, float relativeY,
                                 float relativeZ, float originalRelativeY) const noexcept {
    const std::int32_t x0 = permute(x);
    const std::int32_t x1 = permute(x + 1);
    const std::int32_t xy00 = permute(x0 + y);
    const std::int32_t xy01 = permute(x0 + y + 1);
    const std::int32_t xy10 = permute(x1 + y);
    const std::int32_t xy11 = permute(x1 + y + 1);
    const float d000 = gradDot(permute(xy00 + z), relativeX, relativeY, relativeZ);
    const float d100 = gradDot(permute(xy10 + z), relativeX - 1.0F, relativeY, relativeZ);
    const float d010 = gradDot(permute(xy01 + z), relativeX, relativeY - 1.0F, relativeZ);
    const float d110 = gradDot(permute(xy11 + z), relativeX - 1.0F, relativeY - 1.0F, relativeZ);
    const float d001 = gradDot(permute(xy00 + z + 1), relativeX, relativeY, relativeZ - 1.0F);
    const float d101 = gradDot(permute(xy10 + z + 1), relativeX - 1.0F, relativeY, relativeZ - 1.0F);
    const float d011 = gradDot(permute(xy01 + z + 1), relativeX, relativeY - 1.0F, relativeZ - 1.0F);
    const float d111 = gradDot(permute(xy11 + z + 1), relativeX - 1.0F, relativeY - 1.0F, relativeZ - 1.0F);
    return jm::lerp3(jm::smoothstep(relativeX), jm::smoothstep(originalRelativeY), jm::smoothstep(relativeZ), d000,
                     d100, d010, d110, d001, d101, d011, d111);
}

void PerlinNoise::addToVolume(DensityBuffer& buffer, const DensityVolume& volume, double xzScale, double yScale,
                              float amplitude) const {
    CornerCache corners;
    std::int32_t index = 0;
    for (std::int32_t indexZ = 0; indexZ < volume.sizeZ; ++indexZ) {
        const double z = wrap(static_cast<double>(volume.blockZ(indexZ)) * xzScale) + offsetZ();
        const std::int32_t floorZ = jm::floor(z);
        const auto relativeZ = static_cast<float>(z - floorZ);
        const float alphaZ = jm::smoothstep(relativeZ);
        for (std::int32_t indexX = 0; indexX < volume.sizeX; ++indexX) {
            const double x = wrap(static_cast<double>(volume.blockX(indexX)) * xzScale) + offsetX();
            const std::int32_t floorX = jm::floor(x);
            const auto relativeX = static_cast<float>(x - floorX);
            const std::int32_t x0 = permute(floorX);
            const std::int32_t x1 = permute(floorX + 1);
            const float alphaX = jm::smoothstep(relativeX);
            std::int32_t lastFloorY = std::numeric_limits<std::int32_t>::min();
            for (std::int32_t indexY = 0; indexY < volume.sizeY; ++indexY) {
                const double y = wrap(static_cast<double>(volume.blockY(indexY)) * yScale) + offsetY();
                const std::int32_t floorY = jm::floor(y);
                const auto relativeY = static_cast<float>(y - floorY);
                const float alphaY = jm::smoothstep(relativeY);
                if (lastFloorY != floorY) {
                    fillCorners(*this, corners, x0, x1, floorY, floorZ, relativeX, relativeZ);
                    lastFloorY = floorY;
                }
                buffer.addTo(index, amplitude * lerpCorners(corners, alphaX, alphaY, alphaZ, relativeY));
                ++index;
            }
        }
    }
}

Interval SmearedPerlinNoise::rangeFor(double fudgeYScale) noexcept {
    return Interval::ofSymmetric(static_cast<float>(std::fabs(fudgeYScale) + 2.0));
}

double SmearedPerlinNoise::computeFudgeY(double originalY, double relativeY) const noexcept {
    const double fudgeLimit = originalY >= 0.0 && originalY < relativeY ? originalY : relativeY;
    // parity: the epsilon is the float 1.0E-7F widened, not the double literal.
    return static_cast<double>(jm::floor(fudgeLimit / fudgeYScale_ + static_cast<double>(1.0E-7F))) * fudgeYScale_;
}

float SmearedPerlinNoise::get(double inX, double inY, double inZ) const {
    const double x = wrap(inX) + offsetX();
    const double y = wrap(inY) + offsetY();
    const double z = wrap(inZ) + offsetZ();
    const std::int32_t floorX = jm::floor(x);
    const std::int32_t floorY = jm::floor(y);
    const std::int32_t floorZ = jm::floor(z);
    const auto relativeX = static_cast<float>(x - floorX);
    const double relativeY = y - floorY;
    const auto relativeZ = static_cast<float>(z - floorZ);
    const auto fudgedRelativeY = static_cast<float>(relativeY - computeFudgeY(inY, relativeY));
    return sampleAndLerp(floorX, floorY, floorZ, relativeX, fudgedRelativeY, relativeZ, static_cast<float>(relativeY));
}

void SmearedPerlinNoise::addToVolume(DensityBuffer& buffer, const DensityVolume& volume, double xzScale, double yScale,
                                     float amplitude) const {
    CornerCache corners;
    std::int32_t index = 0;
    for (std::int32_t indexZ = 0; indexZ < volume.sizeZ; ++indexZ) {
        const double z = wrap(static_cast<double>(volume.blockZ(indexZ)) * xzScale) + offsetZ();
        const std::int32_t floorZ = jm::floor(z);
        const auto relativeZ = static_cast<float>(z - floorZ);
        const float alphaZ = jm::smoothstep(relativeZ);
        for (std::int32_t indexX = 0; indexX < volume.sizeX; ++indexX) {
            const double x = wrap(static_cast<double>(volume.blockX(indexX)) * xzScale) + offsetX();
            const std::int32_t floorX = jm::floor(x);
            const auto relativeX = static_cast<float>(x - floorX);
            const std::int32_t x0 = permute(floorX);
            const std::int32_t x1 = permute(floorX + 1);
            const float alphaX = jm::smoothstep(relativeX);
            std::int32_t lastFloorY = std::numeric_limits<std::int32_t>::min();
            for (std::int32_t indexY = 0; indexY < volume.sizeY; ++indexY) {
                const double originalY = static_cast<double>(volume.blockY(indexY)) * yScale;
                const double y = wrap(originalY) + offsetY();
                const std::int32_t floorY = jm::floor(y);
                const double relativeY = y - floorY;
                const float alphaY = jm::smoothstep(static_cast<float>(relativeY));
                if (lastFloorY != floorY) {
                    fillCorners(*this, corners, x0, x1, floorY, floorZ, relativeX, relativeZ);
                    lastFloorY = floorY;
                }
                const auto fudged = static_cast<float>(relativeY - computeFudgeY(originalY, relativeY));
                buffer.addTo(index, amplitude * lerpCorners(corners, alphaX, alphaY, alphaZ, fudged));
                ++index;
            }
        }
    }
}

float SimplexNoise::get(double inX, double inY) const {
    static const double kSqrt3 = std::sqrt(3.0);
    static const double kF2 = 0.5 * (kSqrt3 - 1.0);
    static const double kG2 = (3.0 - kSqrt3) / 6.0;
    const double xin = inX + offsetX();
    const double yin = inY + offsetY();
    const double s = (xin + yin) * kF2;
    const std::int32_t i = jm::floor(xin + s);
    const std::int32_t j = jm::floor(yin + s);
    const double t = static_cast<double>(i + j) * kG2;
    const double x0 = xin - (static_cast<double>(i) - t);
    const double y0 = yin - (static_cast<double>(j) - t);
    const std::int32_t i1 = x0 > y0 ? 1 : 0;
    const std::int32_t j1 = x0 > y0 ? 0 : 1;
    const double x1 = x0 - i1 + kG2;
    const double y1 = y0 - j1 + kG2;
    const double x2 = x0 - 1.0 + 2.0 * kG2;
    const double y2 = y0 - 1.0 + 2.0 * kG2;
    const std::int32_t ii = i & 255;
    const std::int32_t jj = j & 255;
    const std::int32_t gi0 = permute(ii + permute(jj)) % 12;
    const std::int32_t gi1 = permute(ii + i1 + permute(jj + j1)) % 12;
    const std::int32_t gi2 = permute(ii + 1 + permute(jj + 1)) % 12;
    const double n0 = cornerNoise(gi0, x0, y0, 0.0, 0.5);
    const double n1 = cornerNoise(gi1, x1, y1, 0.0, 0.5);
    const double n2 = cornerNoise(gi2, x2, y2, 0.0, 0.5);
    return static_cast<float>(70.0 * (n0 + n1 + n2));
}

float SimplexNoise::get(double inX, double inY, double inZ) const {
    constexpr double kF3 = 0.3333333333333333;
    constexpr double kG3 = 0.16666666666666666;
    const double xin = inX + offsetX();
    const double yin = inY + offsetY();
    const double zin = inZ + offsetZ();
    const double s = (xin + yin + zin) * kF3;
    const std::int32_t i = jm::floor(xin + s);
    const std::int32_t j = jm::floor(yin + s);
    const std::int32_t k = jm::floor(zin + s);
    const double t = static_cast<double>(i + j + k) * kG3;
    const double x0 = xin - (static_cast<double>(i) - t);
    const double y0 = yin - (static_cast<double>(j) - t);
    const double z0 = zin - (static_cast<double>(k) - t);
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
    } else if (y0 < z0) {
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
    const double x1 = x0 - i1 + kG3;
    const double y1 = y0 - j1 + kG3;
    const double z1 = z0 - k1 + kG3;
    const double x2 = x0 - i2 + kF3;
    const double y2 = y0 - j2 + kF3;
    const double z2 = z0 - k2 + kF3;
    const double x3 = x0 - 1.0 + 0.5;
    const double y3 = y0 - 1.0 + 0.5;
    const double z3 = z0 - 1.0 + 0.5;
    const std::int32_t ii = i & 255;
    const std::int32_t jj = j & 255;
    const std::int32_t kk = k & 255;
    const std::int32_t gi0 = permute(ii + permute(jj + permute(kk))) % 12;
    const std::int32_t gi1 = permute(ii + i1 + permute(jj + j1 + permute(kk + k1))) % 12;
    const std::int32_t gi2 = permute(ii + i2 + permute(jj + j2 + permute(kk + k2))) % 12;
    const std::int32_t gi3 = permute(ii + 1 + permute(jj + 1 + permute(kk + 1))) % 12;
    const double n0 = cornerNoise(gi0, x0, y0, z0, 0.6);
    const double n1 = cornerNoise(gi1, x1, y1, z1, 0.6);
    const double n2 = cornerNoise(gi2, x2, y2, z2, 0.6);
    const double n3 = cornerNoise(gi3, x3, y3, z3, 0.6);
    return static_cast<float>(32.0 * (n0 + n1 + n2 + n3));
}

NoiseStack::NoiseStack(std::vector<Layer> layers) : layers_(std::move(layers)), range_(Interval::ofExact(0.0F)) {
    for (const Layer& layer : layers_) {
        range_ = Interval::add(range_, Interval::mul(layer.noise->range(), Interval::ofExact(layer.amplitude)));
    }
}

float NoiseStack::get(double x, double y, double z) const {
    float value = 0.0F;
    for (const Layer& layer : layers_) {
        const double frequency = layer.frequency;
        value += layer.amplitude * layer.noise->get(x * frequency, y * frequency, z * frequency);
    }
    return value;
}

float NoiseStack::get(double x, double y) const {
    float value = 0.0F;
    for (const Layer& layer : layers_) {
        const double frequency = layer.frequency;
        value += layer.amplitude * layer.noise->get(x * frequency, y * frequency);
    }
    return value;
}

void NoiseStack::addToVolume(DensityBuffer& buffer, const DensityVolume& volume, double xzScale, double yScale,
                             float amplitude) const {
    for (const Layer& layer : layers_) {
        const double frequency = layer.frequency;
        layer.noise->addToVolume(buffer, volume, xzScale * frequency, yScale * frequency, amplitude * layer.amplitude);
    }
}

} // namespace basalt::server::world::noise
