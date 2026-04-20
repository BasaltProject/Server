#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/config/CommandsConfig.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `TitleCommand`, all six forms.
class TitleCommand final : public Command {
public:
    TitleCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `ExperienceCommand`, `/xp` alias included.
class ExperienceCommand final : public Command {
public:
    ExperienceCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `SetSpawnCommand`.
class SpawnPointCommand final : public Command {
public:
    SpawnPointCommand();

    void build(const CommandNodeBuilder& root) const override;
};

class ConfiguredCommand final : public Command {
public:
    explicit ConfiguredCommand(config::CustomCommand definition);

    void build(const CommandNodeBuilder& root) const override;

private:
    config::CustomCommand definition_;
};

} // namespace basalt::server::runtime::command::commands
