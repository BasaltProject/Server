#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/commands/FunctionCommands.hpp>
#include <basalt/server/runtime/world/RealDialogs.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealTestInstances.hpp>
#include <basalt/server/runtime/world/StructureTemplates.hpp>

#include <map>
#include <set>

namespace basalt::server::runtime::command::commands {

/// The locator-bar icon of an entity. parity: `WaypointCommand`.
///
/// Every change is pushed with `tracked_waypoint`, so the icon really appears on the bar: the
/// waypoint is the positioned kind, identified by the entity's own UUID, which is how vanilla
/// tracks a player's.
class WaypointCommand final : public Command {
public:
    WaypointCommand();

    void build(const CommandNodeBuilder& root) const override;

private:
    /// Style and colour per entity id, which is what `list` reports and `modify` changes.
    struct Icon {
        wire::common::Identifier style = wire::common::Identifier::vanilla("default");
        std::optional<std::int32_t> color;
    };

    /// Mutable because the tree hands out const callbacks; the icons are the command's own state.
    mutable std::map<std::int32_t, Icon> icons_;
};

/// parity: `PlaceCommand`. `template` works end to end, rotation and mirror included.
///
/// `feature` covers what the placed-feature table exposes by id; the rest, and `jigsaw` and
/// `structure`, are refused by name until generation can be asked for one thing at one position.
class PlaceCommand final : public Command {
public:
    /// `nextSeed` is what a feature's own random is seeded from, so a run is repeatable in a test.
    PlaceCommand(world::StructureTemplates& templates, std::function<std::int64_t()> nextSeed);

    void build(const CommandNodeBuilder& root) const override;

private:
    world::StructureTemplates* templates_;
    std::function<std::int64_t()> nextSeed_;
};

/// parity: `EnchantCommand`.
///
/// The enchantment goes on the held stack as the `minecraft:enchantments` component, which is what
/// the client draws and what `players.db` keeps. It needs the enchantment registry to have been
/// sent, so on a server with no `server.jar` downloaded it refuses by name rather than writing a
/// component nothing can resolve.
class EnchantCommand final : public Command {
public:
    explicit EnchantCommand(const world::RealEnchantments& enchantments);

    void build(const CommandNodeBuilder& root) const override;

private:
    const world::RealEnchantments* enchantments_;
};

/// parity: `DialogCommand`.
///
/// A dialog is a registry entry, so this only works once the `minecraft:dialog` registry has gone
/// out. Without it the command says so rather than naming an id no client knows.
class DialogCommand final : public Command {
public:
    explicit DialogCommand(const world::RealDialogs& dialogs);

    void build(const CommandNodeBuilder& root) const override;

private:
    const world::RealDialogs* dialogs_;
};

/// parity: `TestCommand`, for the kind of test this server can actually run.
///
/// A `minecraft:function` test is a function that fails by failing, which is the kind the vanilla
/// jar itself ships and the only kind a datapack can write without the block-assertion API. A test
/// of any other type is reported as skipped rather than passed, because claiming a pass for
/// something that never ran is the one answer a test framework must never give.
class TestCommand final : public Command {
public:
    TestCommand(const world::RealTestInstances& instances, FunctionRunner& runner);

    void build(const CommandNodeBuilder& root) const override;

private:
    const world::RealTestInstances* instances_;
    FunctionRunner* runner_;
    /// Which tests failed last time, so `runfailed` has something to run.
    mutable std::set<wire::common::Identifier> failed_;
};

} // namespace basalt::server::runtime::command::commands
