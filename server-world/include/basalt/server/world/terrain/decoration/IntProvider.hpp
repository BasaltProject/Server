#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {

/// parity: the `net.minecraft.util.valueproviders.IntProvider` variants the ported worldgen uses.
class IntProvider {
public:
    [[nodiscard]] static IntProvider of(std::int32_t value);

    /// parity: `UniformInt`.
    [[nodiscard]] static IntProvider uniform(std::int32_t min, std::int32_t max);

    /// parity: `ClampedInt`, which clamps the sampled source to an inclusive range.
    [[nodiscard]] static IntProvider clamped(IntProvider source, std::int32_t min, std::int32_t max);

    /// parity: `WeightedListInt` over constant entries.
    [[nodiscard]] static IntProvider weighted(std::initializer_list<std::pair<std::int32_t, std::int32_t>> entries);

    [[nodiscard]] std::int32_t sample(random::RandomSource& random) const;

private:
    enum class Kind : std::uint8_t { Constant, Uniform, Clamped, Weighted };

    Kind kind_ = Kind::Constant;
    std::int32_t first_ = 0;
    std::int32_t second_ = 0;
    std::shared_ptr<const IntProvider> source_;
    std::shared_ptr<const std::vector<std::pair<std::int32_t, std::int32_t>>> entries_;
    std::int32_t totalWeight_ = 0;
};

} // namespace basalt::server::world::terrain::decoration
