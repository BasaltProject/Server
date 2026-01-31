#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// The three command blocks and the cycle behind them.
///
/// parity: `CommandBlock` over `CommandBlockEntity` and `BaseCommandBlock`. What runs the command is
/// not here: this half is the redstone, and the command system is handed in.
namespace commandBlocks {

/// parity: the block **is** the mode. There is no property for it, and `CommandBlockEntity.getMode`
/// reads it off the block.
enum class Mode : std::uint8_t { Impulse, Chain, Repeating };

[[nodiscard]] std::optional<Mode> modeOf(std::string_view path);

/// What running a command costs the caller: the text, and whether it succeeded. It is a required
/// parameter everywhere below, never a defaulted hook: a command block that cannot run its command is
/// not a command block, and an empty callback would make one silently.
using RunCommand = std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z, const std::string& command)>;

/// parity: `CommandBlock.setPoweredAndUpdate`. Only a **change** in the signal does anything, and only
/// a rising one books a turn: a repeating or a chain block never starts itself from redstone.
void onSignal(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool powered);

/// One booked turn. Answers whether this was a command block's turn at all, which is when the
/// comparators around it have to be told.
///
/// parity: `CommandBlock.tick` ends in `updateNeighbourForOutputSignal` whatever it did, not only
/// when something ran: a conditional block whose condition failed puts its `SuccessCount` back to zero
/// and the comparator has to hear that too. A repeating block books its next turn from inside this
/// one, which is what makes it run every tick while it has a signal.
[[nodiscard]] bool tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t gameTime,
                        bool commandBlocksWork, std::int32_t maxChainLength, const RunCommand& run);

/// What a command block's block entity holds the moment it is put down.
///
/// parity: `BaseCommandBlock` and `CommandBlockEntity` write **every** one of these on save whether or
/// not anybody set them, so a block entity that only holds what a command wrote into it is not the
/// same block entity: a world of ours would open in vanilla missing them. `LastExecution` is the one
/// that is left out, and only while it is -1.
[[nodiscard]] wire::nbt::NbtCompound defaults();

/// parity: `CommandBlockEntity.setAutomatic`, which books a turn the moment `auto` goes from off to
/// on for a block that is not powered and is not a chain. `loadAdditional` runs it, so writing the
/// block's data is what starts a repeating command block that nobody has powered.
void onDataLoaded(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool wasAutomatic);

/// parity: `CommandBlock.getAnalogOutputSignal`, which is the `SuccessCount` and nothing else.
[[nodiscard]] std::optional<std::int32_t> comparatorSignal(const wire::nbt::NbtCompound& data);

} // namespace commandBlocks

} // namespace basalt::server::runtime::world
