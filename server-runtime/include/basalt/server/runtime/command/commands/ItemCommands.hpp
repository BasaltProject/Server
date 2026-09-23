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

/// parity: 26.3 `ItemCommands`: `replace`, `fill` and `override` a block's or entities' slots with an
/// item or with what another holder has, and `modify` them with an item modifier.
///
/// The slots are a `SlotRanges` name; a registered or inline slot source is refused by name. Of the
/// entities only a player has slots here.
class ItemCommand final : public Command {
public:
    /// `recipes` is only for the furnace: writing its input slot sets the cooking time. `lootTables`
    /// resolves a modifier named by id; without it only an inline one works.
    explicit ItemCommand(world::RealRecipes* recipes = nullptr, world::RealLootTables* lootTables = nullptr);

    void build(const CommandNodeBuilder& root) const override;

private:
    world::RealRecipes* recipes_ = nullptr;
    world::RealLootTables* lootTables_ = nullptr;
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
