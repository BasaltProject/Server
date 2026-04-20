#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <cstdint>
#include <map>

namespace basalt::server::runtime::command::commands {

/// parity: `RandomCommand`.
class RandomCommand final : public Command {
public:
    explicit RandomCommand(std::function<std::int64_t()> worldSeed);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<std::int64_t()> worldSeed_;
    /// Mutable because `build` hands out const callbacks.
    mutable std::map<wire::common::Identifier, random::RandomSource> sequences_;
};

} // namespace basalt::server::runtime::command::commands
