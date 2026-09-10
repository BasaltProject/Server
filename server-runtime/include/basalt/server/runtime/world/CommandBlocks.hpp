#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
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

/// Runs one command block's line, answering how many times it succeeded: one for an ordinary
/// command, one per branch for a `/execute` that forks. parity: `BaseCommandBlock.performCommand`
/// counts brigadier's callbacks, so `SuccessCount` is a count and not a result. Required everywhere
/// below rather than defaulted: an empty callback would make a command block that runs nothing.
using RunCommand =
    std::function<std::int32_t(std::int32_t x, std::int32_t y, std::int32_t z, const std::string& command)>;

/// parity: `CommandBlock.setPoweredAndUpdate`. Only a **change** in the signal does anything, and only
/// a rising one books a turn: a repeating or a chain block never starts itself from redstone.
void onSignal(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool powered);

/// One booked turn. Answers whether it was a command block's turn at all, which is when the
/// comparators around it have to be told. parity: `CommandBlock.tick` ends in
/// `updateNeighbourForOutputSignal` whatever it did, so a failed condition zeroes `SuccessCount` and
/// the comparator hears that too. A repeating block books its next turn from inside this one.
[[nodiscard]] bool tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t gameTime,
                        bool commandBlocksWork, std::int32_t maxChainLength, const RunCommand& run);

/// Writes what a player typed into a command block's screen, answering the block change when the
/// mode asked for a different block. parity: `handleSetCommandBlock` swaps the block for the mode
/// and keeps the facing. The permission check belongs to the caller.
[[nodiscard]] std::optional<BlockChange> program(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                 const std::string& command, Mode mode, bool trackOutput,
                                                 bool conditional, bool automatic);

/// What a command block's block entity holds the moment it is put down. parity: vanilla writes every
/// one of these on save whether or not anybody set them, so without them a world of ours opens in
/// vanilla missing them. `LastExecution` is left out, and only while it is -1.
[[nodiscard]] wire::nbt::NbtCompound defaults();

/// parity: `CommandBlockEntity.setAutomatic`, which books a turn the moment `auto` goes from off to
/// on for a block that is not powered and is not a chain. `loadAdditional` runs it, so writing the
/// block's data is what starts a repeating command block that nobody has powered.
void onDataLoaded(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool wasAutomatic);

/// parity: `CommandBlock.getAnalogOutputSignal`, which is the `SuccessCount` and nothing else.
[[nodiscard]] std::optional<std::int32_t> comparatorSignal(const wire::nbt::NbtCompound& data);

} // namespace commandBlocks

} // namespace basalt::server::runtime::world
