#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `GiveCommand`.
///
/// Every component the registry knows is accepted and stored. The ones this build has no network
/// codec for are reported by name, the way `/summon` reports the NBT keys it ignores: the item is
/// really carrying them, but no client draws them yet.
class GiveCommand final : public Command {
public:
    GiveCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `ClearInventoryCommands`, including `maxCount` 0, which counts without removing anything
/// and reports through the `commands.clear.test.*` keys instead.
class ClearCommand final : public Command {
public:
    ClearCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
