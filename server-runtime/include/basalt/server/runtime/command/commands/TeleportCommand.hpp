#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <cstdint>

namespace basalt::server::runtime::command::commands {

/// parity: `TeleportCommand`, minus its cross-dimension branches.
class TeleportCommand final : public Command {
public:
    TeleportCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `PlaySoundCommand`.
class PlaySoundCommand final : public Command {
public:
    explicit PlaySoundCommand(std::function<std::int64_t()> nextSeed);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<std::int64_t()> nextSeed_;
};

/// parity: `StopSoundCommand`, whose four success keys say exactly which half was given.
class StopSoundCommand final : public Command {
public:
    StopSoundCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
