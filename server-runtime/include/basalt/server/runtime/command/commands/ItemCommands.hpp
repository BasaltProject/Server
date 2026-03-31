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

/// parity: `GiveCommand`. Every registry component is stored; the ones with no network codec yet are
/// reported by name, as `/summon` reports the NBT keys it ignores.
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

/// parity: 26.3 `ItemCommands`: `replace`, `fill`, `override` and `modify` a block's or entities' slots.
/// Slots are a `SlotRanges` name; a registered or inline slot source is refused by name. Of the
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
/// Each roll runs on the table's own random sequence in the command's world, as vanilla's does. The
/// tool argument is parsed and ignored, which a roll sees as the empty hand vanilla gives it too.
class LootCommand final : public Command {
public:
    explicit LootCommand(world::RealLootTables& lootTables);

    void build(const CommandNodeBuilder& root) const override;

private:
    world::RealLootTables* lootTables_;
};

} // namespace basalt::server::runtime::command::commands
