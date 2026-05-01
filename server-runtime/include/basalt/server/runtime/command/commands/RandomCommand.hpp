#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/world/RandomSequences.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::command::commands {

/// parity: `RandomCommand`.
class RandomCommand final : public Command {
public:
    RandomCommand(std::function<std::int64_t()> worldSeed, std::function<world::RandomSequences&()> sequences);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<std::int64_t()> worldSeed_;
    std::function<world::RandomSequences&()> sequences_;
    /// Mutable because `build` hands out const callbacks; the stream of a draw that names no sequence.
    mutable std::optional<random::RandomSource> anonymous_;
};

} // namespace basalt::server::runtime::command::commands
