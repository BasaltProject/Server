#pragma once

#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/ExecuteCommand.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands::executeCommand {

/// parity: a player's eye height, which is what `anchored eyes` aims from.
inline constexpr double kEyeHeight = 1.62;
inline constexpr double kDegreesPerRadian = 57.2957795130823;

[[nodiscard]] const std::vector<CommandTarget>& targetsOf(const CommandContext& context, std::string_view name);

[[nodiscard]] std::array<std::int32_t, 3> blockPosOf(const CommandContext& context, std::string_view name);

[[nodiscard]] std::shared_ptr<const CommandSource> wrapping(const CommandSource& base,
                                                            ExecutionSource::Changes changes);

/// parity: `Entity.lookAt`: yaw grows clockwise from +Z, pitch is positive downwards.
[[nodiscard]] std::shared_ptr<const CommandSource> facing(const CommandSource& source, double x, double y, double z);

using Test = std::function<std::optional<std::int32_t>(const CommandContext&)>;

/// `terminal` false leaves only the fork, so the line cannot end there. parity: `if function`.
void conditional(const CommandNodeBuilder& builder, CommandNode& execute, bool positive, Test test,
                 bool terminal = true);

using Write = std::function<void(std::int32_t)>;
using WriteFactory = std::function<Write(const CommandContext&)>;

void storing(const CommandNodeBuilder& builder, CommandNode& execute, bool fromSuccess, WriteFactory write);

/// The `<type> <scale>` tail every data store shares. parity: the six number types vanilla writes.
using TypedWrite = std::function<void(double)>;
using TypedWriteFactory = std::function<TypedWrite(const CommandContext&, std::string_view type)>;

void typedStore(const CommandNodeBuilder& builder, CommandNode& execute, bool fromSuccess, TypedWriteFactory write);

} // namespace basalt::server::runtime::command::commands::executeCommand
