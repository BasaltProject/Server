#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <map>
#include <mutex>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `RandomSequences`.
class RandomSequences {
public:
    explicit RandomSequences(std::int64_t worldSeed) : worldSeed_(worldSeed) {}

    RandomSequences(const RandomSequences&) = delete;
    RandomSequences& operator=(const RandomSequences&) = delete;

    [[nodiscard]] random::RandomSource& of(const wire::common::Identifier& name);

    void load(std::string_view worldRoot);

    void save(std::string_view worldRoot);

private:
    std::int64_t worldSeed_ = 0;
    /// Map nodes never move, so a stream handed out stays valid while another one is made.
    std::mutex mutex_;
    std::map<wire::common::Identifier, random::RandomSource> streams_;
    bool dirty_ = false;
};

} // namespace basalt::server::runtime::world
