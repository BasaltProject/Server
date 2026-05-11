#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/commands/FunctionCommands.hpp>
#include <basalt/server/runtime/world/RealDialogs.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealTestInstances.hpp>
#include <basalt/server/runtime/world/StructureTemplates.hpp>

#include <cstdint>
#include <map>
#include <set>

namespace basalt::server::runtime {
class BlockTests;
}

namespace basalt::server::runtime::command::commands {

/// parity: `WaypointCommand`.
class WaypointCommand final : public Command {
public:
    WaypointCommand();

    void build(const CommandNodeBuilder& root) const override;

private:
    struct Icon {
        wire::common::Identifier style = wire::common::Identifier::vanilla("default");
        std::optional<std::int32_t> color;
    };

    /// Mutable because the tree hands out const callbacks; the icons are the command's own state.
    mutable std::map<std::int32_t, Icon> icons_;
};

/// parity: `PlaceCommand`.
class PlaceCommand final : public Command {
public:
    PlaceCommand(world::StructureTemplates& templates, std::function<std::int64_t()> nextSeed);

    void build(const CommandNodeBuilder& root) const override;

private:
    world::StructureTemplates* templates_;
    std::function<std::int64_t()> nextSeed_;
};

/// parity: `EnchantCommand`.
class EnchantCommand final : public Command {
public:
    explicit EnchantCommand(const world::RealEnchantments& enchantments);

    void build(const CommandNodeBuilder& root) const override;

private:
    const world::RealEnchantments* enchantments_;
};

/// parity: `DialogCommand`.
class DialogCommand final : public Command {
public:
    explicit DialogCommand(const world::RealDialogs& dialogs);

    void build(const CommandNodeBuilder& root) const override;

private:
    const world::RealDialogs* dialogs_;
};

/// parity: `TestCommand`, for the kind of test this server can actually run.
class TestCommand final : public Command {
public:
    TestCommand(const world::RealTestInstances& instances, FunctionRunner& runner,
                std::function<BlockTests*()> blockTests);

    void build(const CommandNodeBuilder& root) const override;

private:
    const world::RealTestInstances* instances_;
    FunctionRunner* runner_;
    std::function<BlockTests*()> blockTests_;
    mutable std::set<wire::common::Identifier> failed_;
};

} // namespace basalt::server::runtime::command::commands
