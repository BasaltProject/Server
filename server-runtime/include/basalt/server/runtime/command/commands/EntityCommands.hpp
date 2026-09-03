#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `SummonCommand`.
///
/// What appears is a real entity of the right type at the right place, which is what the client
/// draws and what the other entity commands then act on. It has no AI and does not move on its own:
/// behaviour is the entity work, not the command's, and pretending otherwise here would be worse
/// than the gap.
class SummonCommand final : public Command {
public:
    explicit SummonCommand(player::PlayerRegistry& players);

    void build(const CommandNodeBuilder& root) const override;

private:
    player::PlayerRegistry* players_;
};

/// parity: `DamageCommand`, minus the damage type, which needs the damage type registry to matter.
class DamageCommand final : public Command {
public:
    /// `damageTypes` is what the completion lists, or unset where no jar is loaded.
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

/// parity: `RotateCommand`, all three forms: an absolute rotation, facing a point, facing an entity.
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
///
/// A modifier is applied in vanilla's own order (`add_value`, then `add_multiplied_base` against
/// the base, then `add_multiplied_total` against the running total), so `get` reports the same
/// number a real server would.
class AttributeCommand final : public Command {
public:
    AttributeCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
