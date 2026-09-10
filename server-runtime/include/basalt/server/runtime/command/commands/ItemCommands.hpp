#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>

#include <cstdint>
#include <functional>

/// Declared, not included: the command only ever holds a pointer to one.
namespace basalt::server::runtime::world {
class RealLootTables;
}

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

/// parity: `ItemCommands`.
///
/// `replace` and the `from` copies work on both a player and a container. The `modify` forms are
/// declared and refuse by name: an item modifier is a loot function, and none are loaded here, so
/// accepting one would copy the stack unchanged and report a success that never happened.
class ItemCommand final : public Command {
public:
    /// `recipes` is only for the furnace: writing its input slot sets the cooking time, which is what
    /// `setItem` does in the jar. Without one a furnace keeps whatever time it had.
    explicit ItemCommand(world::RealRecipes* recipes = nullptr);

    void build(const CommandNodeBuilder& root) const override;

private:
    world::RealRecipes* recipes_ = nullptr;
};

/// parity: `LootCommand`: four destinations and four sources, each pairing the same way.
///
/// `mine` and `loot` roll a real table; `kill` rolls the entity's own; `fish` rolls the table it is
/// given, since fishing itself does not exist here. The tool argument is parsed and ignored: loot
/// conditions on it are not modelled, so it would change nothing.
class LootCommand final : public Command {
public:
    /// The roll is seeded from the world seed rather than from a live random, so the same command at
    /// the same place answers the same thing twice.
    LootCommand(world::RealLootTables& lootTables, std::function<std::int64_t()> worldSeed);

    void build(const CommandNodeBuilder& root) const override;

private:
    world::RealLootTables* lootTables_;
    std::function<std::int64_t()> worldSeed_;
};

} // namespace basalt::server::runtime::command::commands
