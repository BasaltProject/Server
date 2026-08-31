#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::world::testsupport {

/// A block state by its vanilla path, for tests that name blocks rather than ids.
[[nodiscard]] inline std::int32_t stateIdOf(std::string_view path) { return registry::blocks::named(path)->stateId; }

/// The surface environment the decoration tests run against: one fixed biome and flat answers for
/// everything a step-9 feature asks. Fourteen tests had a byte-identical copy of this before it was
/// pulled out here, which is both a lot of duplication and a lot of ways for the copies to drift.
class FixedBiomeEnvironment final : public terrain::surface::SurfaceRuleEnvironment {
public:
    explicit FixedBiomeEnvironment(std::string biome, std::int32_t surfaceHeight = 70)
        : biome_(std::move(biome)), surfaceHeight_(surfaceHeight) {}

    [[nodiscard]] std::int32_t surfaceDepthNoise(std::int32_t, std::int32_t) const override { return 0; }

    [[nodiscard]] double surfaceSecondaryNoise(std::int32_t, std::int32_t) const override { return 0.0; }

    [[nodiscard]] double sample2d(std::string_view, std::int32_t, std::int32_t) const override { return 0.0; }

    [[nodiscard]] double sample3d(std::string_view, std::int32_t, std::int32_t, std::int32_t) const override {
        return 0.0;
    }

    [[nodiscard]] float verticalGradientRandom(std::string_view, std::int32_t, std::int32_t,
                                               std::int32_t) const override {
        return 0.5F;
    }

    [[nodiscard]] random::RandomSource positionalRandom(std::int32_t x, std::int32_t, std::int32_t z) const override {
        return random::RandomSource::xoroshiro(x * 31LL + z);
    }

    [[nodiscard]] std::int32_t estimatedSurfaceHeight(std::int32_t, std::int32_t) const override {
        return surfaceHeight_;
    }

    [[nodiscard]] const std::string& biomeAt(std::int32_t, std::int32_t, std::int32_t) const override { return biome_; }

    [[nodiscard]] const std::string& noiseBiomeAt(std::int32_t, std::int32_t, std::int32_t,
                                                  densityfunction::DensityCache*) const override {
        return biome_;
    }

    [[nodiscard]] bool isSteep(std::int32_t, std::int32_t) const override { return false; }

    [[nodiscard]] std::int32_t clayBand(std::int32_t, std::int32_t, std::int32_t) const override { return 0; }

private:
    std::string biome_;
    std::int32_t surfaceHeight_;
};

/// FNV-1a over a y range of a chunk. Every decoration test folds its result this way, so the
/// constants live in one place rather than being retyped per test.
class Checksum {
public:
    void feed(std::int64_t value) noexcept { value_ = (value_ ^ static_cast<std::uint64_t>(value)) * 1099511628211ULL; }

    [[nodiscard]] std::int64_t value() const noexcept { return static_cast<std::int64_t>(value_); }

private:
    std::uint64_t value_ = 14695981039346656037ULL;
};

} // namespace basalt::server::world::testsupport
