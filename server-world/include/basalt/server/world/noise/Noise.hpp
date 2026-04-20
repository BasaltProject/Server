#pragma once

#include <basalt/server/world/densityfunction/DensityVolume.hpp>
#include <basalt/server/world/noise/Interval.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::world::noise {

/// parity: `synth.Noise`.
class Noise {
public:
    virtual ~Noise() = default;
    Noise(const Noise&) = delete;
    Noise& operator=(const Noise&) = delete;
    Noise(Noise&&) = delete;
    Noise& operator=(Noise&&) = delete;

    [[nodiscard]] virtual Interval range() const = 0;

    [[nodiscard]] virtual float get(double x, double y) const = 0;

    [[nodiscard]] virtual float get(double x, double y, double z) const = 0;

    /// parity: `Noise.addToVolume`, the default that samples point by point.
    virtual void addToVolume(densityfunction::DensityBuffer& buffer, const densityfunction::DensityVolume& volume,
                             double xzScale, double yScale, float amplitude) const;

protected:
    Noise() = default;
};

/// parity: `GradientNoise`, the permutation table and offsets both gradient noises share.
class GradientNoise : public Noise {
public:
    struct Gradient {
        std::int32_t x;
        std::int32_t y;
        std::int32_t z;

        [[nodiscard]] double dot(double dx, double dy, double dz) const noexcept {
            return static_cast<double>(x) * dx + static_cast<double>(y) * dy + static_cast<double>(z) * dz;
        }

        [[nodiscard]] float dot(float dx, float dy, float dz) const noexcept {
            return static_cast<float>(x) * dx + static_cast<float>(y) * dy + static_cast<float>(z) * dz;
        }

        [[nodiscard]] float dotXz(float dx, float dz) const noexcept {
            return static_cast<float>(x) * dx + static_cast<float>(z) * dz;
        }
    };

    static constexpr std::array<Gradient, 16> kGradients{{{1, 1, 0},
                                                          {-1, 1, 0},
                                                          {1, -1, 0},
                                                          {-1, -1, 0},
                                                          {1, 0, 1},
                                                          {-1, 0, 1},
                                                          {1, 0, -1},
                                                          {-1, 0, -1},
                                                          {0, 1, 1},
                                                          {0, -1, 1},
                                                          {0, 1, -1},
                                                          {0, -1, -1},
                                                          {1, 1, 0},
                                                          {0, -1, 1},
                                                          {-1, 1, 0},
                                                          {0, -1, -1}}};

    [[nodiscard]] double offsetX() const noexcept { return offsetX_; }

    [[nodiscard]] double offsetY() const noexcept { return offsetY_; }

    [[nodiscard]] double offsetZ() const noexcept { return offsetZ_; }

    [[nodiscard]] std::int32_t permute(std::int32_t x) const noexcept {
        return perms_[static_cast<std::size_t>(static_cast<std::uint32_t>(x) & 255U)];
    }

    /// parity: `GradientNoise.wrap`.
    [[nodiscard]] static double wrap(double x) noexcept;

protected:
    GradientNoise(random::RandomSource& random, double noiseOffsetScale);

    [[nodiscard]] const Gradient& permuteToGrad(std::int32_t x) const noexcept {
        return kGradients[static_cast<std::size_t>(permute(x) & 15)];
    }

    [[nodiscard]] static float gradDot(std::int32_t hash, float x, float y, float z) noexcept {
        return kGradients[static_cast<std::size_t>(hash & 15)].dot(x, y, z);
    }

private:
    std::array<std::uint8_t, 256> perms_{};
    double offsetX_ = 0.0;
    double offsetY_ = 0.0;
    double offsetZ_ = 0.0;
};

/// parity: `synth.PerlinNoise`, one octave of classic Perlin noise in float.
class PerlinNoise : public GradientNoise {
public:
    explicit PerlinNoise(random::RandomSource& random) : GradientNoise(random, 256.0) {}

    [[nodiscard]] Interval range() const override { return Interval::ofSymmetric(2.0F); }

    [[nodiscard]] float get(double x, double y) const override;

    [[nodiscard]] float get(double x, double y, double z) const override;

    void addToVolume(densityfunction::DensityBuffer& buffer, const densityfunction::DensityVolume& volume,
                     double xzScale, double yScale, float amplitude) const override;

protected:
    [[nodiscard]] float sampleAndLerp(std::int32_t x, std::int32_t y, std::int32_t z, float relativeX, float relativeY,
                                      float relativeZ, float originalRelativeY) const noexcept;
};

/// parity: `synth.SmearedPerlinNoise`.
class SmearedPerlinNoise final : public PerlinNoise {
public:
    SmearedPerlinNoise(random::RandomSource& random, double fudgeYScale)
        : PerlinNoise(random), fudgeYScale_(fudgeYScale) {}

    [[nodiscard]] static Interval rangeFor(double fudgeYScale) noexcept;

    [[nodiscard]] Interval range() const override { return rangeFor(fudgeYScale_); }

    [[nodiscard]] float get(double x, double y, double z) const override;

    using PerlinNoise::get;

    void addToVolume(densityfunction::DensityBuffer& buffer, const densityfunction::DensityVolume& volume,
                     double xzScale, double yScale, float amplitude) const override;

private:
    [[nodiscard]] double computeFudgeY(double originalY, double relativeY) const noexcept;

    double fudgeYScale_;
};

/// parity: `synth.SimplexNoise`.
class SimplexNoise final : public GradientNoise {
public:
    SimplexNoise(random::RandomSource& random, bool discardNoiseOffset)
        : GradientNoise(random, discardNoiseOffset ? 0.0 : 256.0) {}

    [[nodiscard]] Interval range() const override { return Interval::ofSymmetric(2.0F); }

    [[nodiscard]] float get(double x, double y) const override;

    [[nodiscard]] float get(double x, double y, double z) const override;
};

/// parity: `synth.NoiseStack`.
class NoiseStack final : public Noise {
public:
    struct Layer {
        std::shared_ptr<const Noise> noise;
        double frequency;
        float amplitude;
    };

    explicit NoiseStack(std::vector<Layer> layers);

    [[nodiscard]] Interval range() const override { return range_; }

    [[nodiscard]] float get(double x, double y) const override;

    [[nodiscard]] float get(double x, double y, double z) const override;

    void addToVolume(densityfunction::DensityBuffer& buffer, const densityfunction::DensityVolume& volume,
                     double xzScale, double yScale, float amplitude) const override;

    [[nodiscard]] const std::vector<Layer>& layers() const noexcept { return layers_; }

private:
    std::vector<Layer> layers_;
    Interval range_;
};

} // namespace basalt::server::world::noise
