#pragma once

#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/ExecuteCommand.hpp>

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

/// Shared between the three files `/execute` is split across: the tree is a hundred nodes and the
/// three halves have nothing in common but these.
namespace basalt::server::runtime::command::commands::executeCommand {

/// parity: a player's eye height, which is what `anchored eyes` aims from.
inline constexpr double kEyeHeight = 1.62;
inline constexpr double kDegreesPerRadian = 57.2957795130823;

[[nodiscard]] const std::vector<CommandTarget>& targetsOf(const CommandContext& context, std::string_view name);

/// The block coordinates an argument named, resolved against the source running the chain.
[[nodiscard]] std::array<std::int32_t, 3> blockPosOf(const CommandContext& context, std::string_view name);

/// One source wrapping another, as a value the tree can hand back.
[[nodiscard]] std::shared_ptr<const CommandSource> wrapping(const CommandSource& base,
                                                            ExecutionSource::Changes changes);

/// parity: `Entity.lookAt`: yaw grows clockwise from +Z, pitch is positive downwards.
[[nodiscard]] std::shared_ptr<const CommandSource> facing(const CommandSource& source, double x, double y, double z);

/// A condition node: it gates the rest of the line, and reports on its own when the line ends
/// there. parity: `ExecuteCommand.addConditional`, which declares both on the same node.
using Test = std::function<std::optional<std::int32_t>(const CommandContext&)>;

void conditional(const CommandNodeBuilder& builder, CommandNode& execute, bool positive, Test test);

/// A store node: wraps the source so whatever the chain ends up running reports back here.
///
/// The write is built when the link is *parsed*, not when it fires: the callback runs after the
/// whole chain has finished, by which time the context that held this link's arguments is gone, so
/// whatever the write needs has to be copied out of it first.
using Write = std::function<void(std::int32_t)>;
using WriteFactory = std::function<Write(const CommandContext&)>;

void storing(const CommandNodeBuilder& builder, CommandNode& execute, bool fromSuccess, WriteFactory write);

/// The `<type> <scale>` tail every data store shares. parity: the six number types vanilla writes.
using TypedWrite = std::function<void(double)>;
using TypedWriteFactory = std::function<TypedWrite(const CommandContext&, std::string_view type)>;

void typedStore(const CommandNodeBuilder& builder, CommandNode& execute, bool fromSuccess, TypedWriteFactory write);

} // namespace basalt::server::runtime::command::commands::executeCommand
