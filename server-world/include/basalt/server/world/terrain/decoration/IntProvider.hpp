#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {

/// parity: the `net.minecraft.util.valueproviders.IntProvider` variants the ported worldgen uses.
///
/// A value type rather than a class hierarchy: these are copied into feature configs by the
/// hundred, and every variant is two ints plus, at most, a shared child.
class IntProvider {
public:
    [[nodiscard]] static IntProvider of(std::int32_t value);

    /// parity: `UniformInt`, inclusive on both ends. Always draws, even when min equals max, which
    /// is what `Mth.randomBetweenInclusive` does.
    [[nodiscard]] static IntProvider uniform(std::int32_t min, std::int32_t max);

    /// parity: `ClampedInt`, which clamps the sampled source to an inclusive range.
    [[nodiscard]] static IntProvider clamped(IntProvider source, std::int32_t min, std::int32_t max);

    /// parity: `WeightedListInt` over constant entries, the only shape the real configs use. The
    /// pairs stay in insertion order, so a cumulative walk matches vanilla's repeat-by-weight layout.
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
