#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `CommandBlock` over `CommandBlockEntity`.
namespace commandBlocks {

/// parity: `CommandBlockEntity.getMode`.
enum class Mode : std::uint8_t { Impulse, Chain, Repeating };

[[nodiscard]] std::optional<Mode> modeOf(std::string_view path);

/// Returns how many times the command succeeded; parity: `BaseCommandBlock.performCommand` counts brigadier callbacks.
using RunCommand =
    std::function<std::int32_t(std::int32_t x, std::int32_t y, std::int32_t z, const std::string& command)>;

/// parity: `CommandBlock.setPoweredAndUpdate`.
void onSignal(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool powered);

/// parity: `CommandBlock.tick`.
[[nodiscard]] bool tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t gameTime,
                        bool commandBlocksWork, std::int32_t maxChainLength, const RunCommand& run);

/// parity: `handleSetCommandBlock` swaps the block for the mode and keeps the facing.
[[nodiscard]] std::optional<BlockChange> program(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                 const std::string& command, Mode mode, bool trackOutput,
                                                 bool conditional, bool automatic);

/// parity: vanilla writes all of these on save even when unset; `LastExecution` is left out only while -1.
[[nodiscard]] wire::nbt::NbtCompound defaults();

/// parity: `CommandBlockEntity.setAutomatic` books a turn when `auto` turns on for an unpowered non-chain block.
void onDataLoaded(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool wasAutomatic);

/// parity: `CommandBlock.getAnalogOutputSignal`.
[[nodiscard]] std::optional<std::int32_t> comparatorSignal(const wire::nbt::NbtCompound& data);

} // namespace commandBlocks

} // namespace basalt::server::runtime::world
