#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <map>

namespace basalt::server::runtime::command::commands {

/// parity: `RandomCommand`.
///
/// Named sequences are what make a datapack's randomness reproducible: each one is its own stream,
/// seeded from the world seed and the name, and `/random reset` puts it back to its start. `value`
/// is the quiet form and `roll` is the one everybody sees.
class RandomCommand final : public Command {
public:
    explicit RandomCommand(std::function<std::int64_t()> worldSeed);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<std::int64_t()> worldSeed_;
    /// Mutable because a draw advances the stream and `build` hands out const callbacks; the
    /// sequences are the command's own state, not the tree's.
    mutable std::map<wire::common::Identifier, random::RandomSource> sequences_;
};

} // namespace basalt::server::runtime::command::commands
