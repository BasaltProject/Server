#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

class WorldState;

/// parity: `SculkSpreader.createLevelSpreader`.
namespace sculkSpreading {

inline constexpr std::int32_t kMaxCharge = 1000;
inline constexpr std::size_t kMaxCursors = 32;
inline constexpr double kListenerRadius = 8.0;
inline constexpr std::int32_t kBloomTicks = 8;

/// parity: `SculkSpreader.ChargeCursor`.
struct Cursor {
    std::array<std::int32_t, 3> pos{};
    std::int32_t charge = 0;
    std::int32_t decayDelay = 1;
    std::int32_t updateDelay = 0;
    std::optional<std::uint8_t> facings;
};

struct Step {
    std::array<std::int32_t, 3> pos{};
    std::optional<std::int32_t> stateId;
    /// parity: `setBlockAndUpdate` (three) against the multiface spreader's `setBlock(.
    bool updatesNeighbours = true;
    std::optional<BlockEffect> effect;
};

using BlockTagged = std::function<bool(std::string_view tag, std::int32_t stateId)>;

[[nodiscard]] std::vector<Cursor> read(const wire::nbt::NbtCompound& data);
void write(const std::vector<Cursor>& cursors, wire::nbt::NbtCompound& data);

/// parity: `SculkSpreader.addCursors`.
void addCursors(std::vector<Cursor>& cursors, const std::array<std::int32_t, 3>& start, std::int32_t charge);

/// parity: `SculkSpreader.updateCursors` with veins spreading, as the catalyst ticks it.
void update(WorldState& world, std::vector<Cursor>& cursors, const std::array<std::int32_t, 3>& origin,
            random::RandomSource& random, const BlockTagged& tagged, std::vector<Step>& steps);

/// parity: `SculkCatalystBlock.tick`.
[[nodiscard]] std::optional<std::int32_t> unbloomed(std::int32_t stateId);

/// parity: the bloom `CatalystListener.bloom` sets.
[[nodiscard]] std::optional<std::int32_t> bloomed(std::int32_t stateId);

} // namespace sculkSpreading

} // namespace basalt::server::runtime::world
