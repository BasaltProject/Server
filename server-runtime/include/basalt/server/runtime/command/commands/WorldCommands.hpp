#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `MsgCommand`.
class MsgCommand final : public Command {
public:
    MsgCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `WeatherCommand`.
class WeatherCommand final : public Command {
public:
    WeatherCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `DifficultyCommand`.
class DifficultyCommand final : public Command {
public:
    DifficultyCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `SetWorldSpawnCommand`.
class SetWorldSpawnCommand final : public Command {
public:
    SetWorldSpawnCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
