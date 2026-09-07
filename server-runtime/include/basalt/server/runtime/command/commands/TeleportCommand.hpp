#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <cstdint>

namespace basalt::server::runtime::command::commands {

/// parity: `TeleportCommand`, minus its cross-dimension branches.
///
/// The three forms keep vanilla's order, which is load-bearing: `<destination>` and `<targets>`
/// both start by parsing a selector, so `/tp Steve 10 64 10` is only reached by failing the first
/// and backtracking.
///
/// The trailing `<world>` is Basalt's, since vanilla changes world through `/execute`. It is
/// appended rather than inserted so every vanilla form still means what it means in vanilla.
class TeleportCommand final : public Command {
public:
    TeleportCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `PlaySoundCommand`, including the distance rule: a player further away than the sound
/// carries hears it moved towards them at the minimum volume instead of not at all.
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
