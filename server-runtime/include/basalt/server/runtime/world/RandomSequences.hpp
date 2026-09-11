#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <map>
#include <mutex>

namespace basalt::server::runtime::world {

/// The named random streams a world keeps, one per name, each carrying on where it left off.
///
/// parity: `RandomSequences` over `RandomSequence`. A loot table names its own stream, so the same
/// world always drops the same thing in the same order however many other tables roll in between.
/// The seed is the world's own mixed with the name: two worlds drop differently and one world drops
/// the same twice.
///
/// What is missing: vanilla saves them (`random_sequences.dat`), so a stream survives a restart.
/// These start over, which is the same thing for a world opened once and a gap for one reopened.
class RandomSequences {
public:
    explicit RandomSequences(std::int64_t worldSeed) : worldSeed_(worldSeed) {}

    RandomSequences(const RandomSequences&) = delete;
    RandomSequences& operator=(const RandomSequences&) = delete;

    /// The stream of that name, made on first use. parity: `RandomSequences.get`.
    [[nodiscard]] random::RandomSource& of(const wire::common::Identifier& name);

private:
    std::int64_t worldSeed_ = 0;
    /// Map nodes never move, so a stream handed out stays valid while another one is made.
    std::mutex mutex_;
    std::map<wire::common::Identifier, random::RandomSource> streams_;
};

} // namespace basalt::server::runtime::world
