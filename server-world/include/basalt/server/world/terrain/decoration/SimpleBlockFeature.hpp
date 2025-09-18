#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <cstdint>
#include <functional>
#include <utility>

namespace basalt::server::world::terrain::decoration {

/// parity: `SimpleBlockFeature`, which places a single block at the origin if it can survive there.
///
/// Vanilla's `canSurvive` dispatches to each block's own placement rule: overworld vegetation to
/// `supports_vegetation`, dry vegetation to `supports_dry_vegetation`, lily pads to water and ice,
/// mushrooms to their solid-render support, leaf litter to its sturdy-upper-face rule; families with
/// no rule of their own keep the generic solid-ground fallback.
class SimpleBlockFeature {
public:
    using StateProvider = std::function<std::int32_t(random::RandomSource&, const DecorationPos&)>;

    explicit SimpleBlockFeature(StateProvider toPlace, bool isDoublePlant = false, bool scheduleTick = false)
        : toPlace_(std::move(toPlace)), isDoublePlant_(isDoublePlant), scheduleTick_(scheduleTick) {}

    /// `mossyCarpetRandom` exists because the pale moss carpet draws from a different source than
    /// the state provider does.
    bool place(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) const {
        return place(chunk, random, origin, random);
    }

    bool place(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
               random::RandomSource& mossyCarpetRandom) const;

private:
    StateProvider toPlace_;
    bool isDoublePlant_;
    bool scheduleTick_;
};

} // namespace basalt::server::world::terrain::decoration
