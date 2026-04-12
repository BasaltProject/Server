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

/// A sculk catalyst's spreader: charge cursors that walk the sculk and turn what they cross.
///
/// parity: `SculkSpreader.createLevelSpreader` with `SculkBlock`, `SculkVeinBlock` and
/// `SculkBehaviour.DEFAULT` as what each cursor stands on.
namespace sculkSpreading {

inline constexpr std::int32_t kMaxCharge = 1000;
inline constexpr std::size_t kMaxCursors = 32;
/// parity: `CatalystListener.getListenerRadius` and `PULSE_TICKS`.
inline constexpr double kListenerRadius = 8.0;
inline constexpr std::int32_t kBloomTicks = 8;

/// parity: `SculkSpreader.ChargeCursor`; the facings are a bit per direction, absent until first set.
struct Cursor {
    std::array<std::int32_t, 3> pos{};
    std::int32_t charge = 0;
    std::int32_t decayDelay = 1;
    std::int32_t updateDelay = 0;
    std::optional<std::uint8_t> facings;
};

/// One thing the spreader did, in order: a block written with vanilla's flags, or a sound or level
/// event at a square.
struct Step {
    std::array<std::int32_t, 3> pos{};
    std::optional<std::int32_t> stateId;
    /// parity: `setBlockAndUpdate` (three) against the multiface spreader's `setBlock(..., 2)`.
    bool updatesNeighbours = true;
    std::optional<BlockEffect> effect;
};

/// Whether a block state is in a block tag.
using BlockTagged = std::function<bool(std::string_view tag, std::int32_t stateId)>;

[[nodiscard]] std::vector<Cursor> read(const wire::nbt::NbtCompound& data);
void write(const std::vector<Cursor>& cursors, wire::nbt::NbtCompound& data);

/// parity: `SculkSpreader.addCursors`, a thousand of charge at most to a cursor and 32 cursors.
void addCursors(std::vector<Cursor>& cursors, const std::array<std::int32_t, 3>& start, std::int32_t charge);

/// parity: `SculkSpreader.updateCursors` with veins spreading, as the catalyst ticks it.
void update(WorldState& world, std::vector<Cursor>& cursors, const std::array<std::int32_t, 3>& origin,
            random::RandomSource& random, const BlockTagged& tagged, std::vector<Step>& steps);

/// parity: `SculkCatalystBlock.tick`, the bloom let go of. Answers the new state, if it changed.
[[nodiscard]] std::optional<std::int32_t> unbloomed(std::int32_t stateId);

/// parity: the bloom `CatalystListener.bloom` sets, or nothing for a state that is no catalyst.
[[nodiscard]] std::optional<std::int32_t> bloomed(std::int32_t stateId);

} // namespace sculkSpreading

} // namespace basalt::server::runtime::world
