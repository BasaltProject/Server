#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {

/// parity: `BellBlockEntity`'s ringing clock, kept in the block entity data only while the bell rings.
namespace bells {

inline constexpr std::int32_t kDurationTicks = 50;
inline constexpr std::int32_t kTicksBeforeResonation = 5;
inline constexpr std::int32_t kMaxResonationTicks = 40;
inline constexpr std::int32_t kGlowTicks = 60;
inline constexpr double kHearRadius = 32.0;
inline constexpr double kHighlightRadius = 48.0;

struct State {
    std::int32_t ticks = 0;
    bool shaking = false;
    bool resonating = false;
    std::int32_t resonationTicks = 0;
};

struct Step {
    bool startedResonating = false;
    bool endedResonating = false;
};

[[nodiscard]] State read(const wire::nbt::NbtCompound& data);

void write(const State& state, wire::nbt::NbtCompound& data);

[[nodiscard]] bool ringing(const wire::nbt::NbtCompound& data);

/// parity: `BellBlockEntity.triggerEvent` for event one.
void ring(wire::nbt::NbtCompound& data);

void ringAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `BellBlockEntity.tick`.
[[nodiscard]] Step advance(State& state, bool raidersNearby);

} // namespace bells

} // namespace basalt::server::runtime::world
