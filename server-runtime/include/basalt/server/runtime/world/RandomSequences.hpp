#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <map>
#include <mutex>
#include <string_view>

namespace basalt::server::runtime::world {

/// The named random streams a world keeps, one per name, each carrying on where it left off.
///
/// parity: `RandomSequences` over `RandomSequence`. The seed is the world's own mixed with the name,
/// so the same world always drops the same thing in the same order. They survive a restart the way
/// `random_sequences.dat` does, instead of dropping the same thing again.
class RandomSequences {
public:
    explicit RandomSequences(std::int64_t worldSeed) : worldSeed_(worldSeed) {}

    RandomSequences(const RandomSequences&) = delete;
    RandomSequences& operator=(const RandomSequences&) = delete;

    /// The stream of that name, made on first use. parity: `RandomSequences.get`.
    [[nodiscard]] random::RandomSource& of(const wire::common::Identifier& name);

    /// Reads `random_sequences.nbt`, where each name carries the two longs of its generator.
    void load(std::string_view worldRoot);

    void save(std::string_view worldRoot);

private:
    std::int64_t worldSeed_ = 0;
    /// Map nodes never move, so a stream handed out stays valid while another one is made.
    std::mutex mutex_;
    std::map<wire::common::Identifier, random::RandomSource> streams_;
    /// parity: vanilla hands out a source that marks the save dirty on every draw. Handing out the
    /// stream at all is close enough: whoever asked for it asked in order to draw from it.
    bool dirty_ = false;
};

} // namespace basalt::server::runtime::world
