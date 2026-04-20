#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <cstdint>
#include <functional>
#include <utility>

namespace basalt::server::world::terrain::decoration {

/// parity: `SimpleBlockFeature`.
class SimpleBlockFeature {
public:
    using StateProvider = std::function<std::int32_t(random::RandomSource&, const DecorationPos&)>;

    explicit SimpleBlockFeature(StateProvider toPlace, bool isDoublePlant = false, bool scheduleTick = false)
        : toPlace_(std::move(toPlace)), isDoublePlant_(isDoublePlant), scheduleTick_(scheduleTick) {}

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
