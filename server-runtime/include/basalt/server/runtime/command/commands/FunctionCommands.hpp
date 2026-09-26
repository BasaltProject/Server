#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandStorage.hpp>
#include <basalt/server/runtime/command/commands/FunctionRunner.hpp>
#include <basalt/server/runtime/function/FunctionLibrary.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::runtime::command::commands {

/// parity: `FunctionCommand`, tags and macro arguments included.
class FunctionCommand final : public Command {
public:
    /// `data` is where `with block|entity|storage` reads its arguments, or null in a fixture
    /// without any.
    FunctionCommand(FunctionRunner& runner, const DataAccess* data = nullptr);

    void build(const CommandNodeBuilder& root) const override;

private:
    FunctionRunner* runner_;
    const DataAccess* data_;
};

/// parity: `ReturnCommand`. Outside a function it only ends the command, as vanilla's top frame does.
class ReturnCommand final : public Command {
public:
    explicit ReturnCommand(FunctionRunner& runner);

    void build(const CommandNodeBuilder& root) const override;

private:
    FunctionRunner* runner_;
};

/// parity: `ScheduleCommand`, keyed on the game time of the world the command runs in.
class ScheduleCommand final : public Command {
public:
    using GameTime = std::function<std::int64_t(const CommandContext& context)>;

    ScheduleCommand(const function::FunctionLibrary& library, FunctionSchedule& schedule, GameTime gameTime);

    void build(const CommandNodeBuilder& root) const override;

private:
    const function::FunctionLibrary* library_;
    FunctionSchedule* schedule_;
    GameTime gameTime_;
};

/// The functions and function tags a `minecraft:function` argument completes with.
[[nodiscard]] std::vector<std::string> functionSuggestions(const function::FunctionLibrary& library);

} // namespace basalt::server::runtime::command::commands
