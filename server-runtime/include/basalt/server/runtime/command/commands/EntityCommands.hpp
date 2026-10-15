#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `SummonCommand`.
class SummonCommand final : public Command {
public:
    explicit SummonCommand(player::PlayerRegistry& players);

    void build(const CommandNodeBuilder& root) const override;

private:
    player::PlayerRegistry* players_;
};

/// parity: `DamageCommand`.
class DamageCommand final : public Command {
public:
    explicit DamageCommand(std::function<std::vector<wire::common::Identifier>()> damageTypes = {});

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<std::vector<wire::common::Identifier>()> damageTypes_;
};

/// parity: `RideCommand`, whose two forms are mounting and dismounting.
class RideCommand final : public Command {
public:
    RideCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `RotateCommand`.
class RotateCommand final : public Command {
public:
    RotateCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `SwingCommand`, which is the arm swing everyone else sees.
class SwingCommand final : public Command {
public:
    SwingCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `AttributeCommand`, base values and modifiers.
class AttributeCommand final : public Command {
public:
    AttributeCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
