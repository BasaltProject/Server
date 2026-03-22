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

/// The biome environment the decoration tests run against: one fixed biome everywhere.
class FixedBiomeEnvironment final : public terrain::surface::SurfaceRuleEnvironment {
public:
    explicit FixedBiomeEnvironment(std::string biome) : biome_(std::move(biome)) {}

    [[nodiscard]] const std::string& biomeAt(std::int32_t, std::int32_t, std::int32_t) const override { return biome_; }

    [[nodiscard]] const std::string& noiseBiomeAt(std::int32_t, std::int32_t, std::int32_t,
                                                  densityfunction::SamplerContext*) const override {
        return biome_;
    }

private:
    std::string biome_;
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
