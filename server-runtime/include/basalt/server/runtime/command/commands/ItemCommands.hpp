#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::runtime::world {
class RealLootTables;
}

namespace basalt::server::runtime::command::commands {

/// parity: `GiveCommand`.
class GiveCommand final : public Command {
public:
    GiveCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `ClearInventoryCommands`.
class ClearCommand final : public Command {
public:
    ClearCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: 26.3 `ItemCommands`.
class ItemCommand final : public Command {
public:
    explicit ItemCommand(world::RealRecipes* recipes = nullptr, world::RealLootTables* lootTables = nullptr);

    void build(const CommandNodeBuilder& root) const override;

private:
    world::RealRecipes* recipes_ = nullptr;
    world::RealLootTables* lootTables_ = nullptr;
};

/// parity: `LootCommand`: four destinations and four sources, each pairing the same way.
class LootCommand final : public Command {
public:
    explicit LootCommand(world::RealLootTables& lootTables);

    void build(const CommandNodeBuilder& root) const override;

private:
    world::RealLootTables* lootTables_;
};

} // namespace basalt::server::runtime::command::commands
