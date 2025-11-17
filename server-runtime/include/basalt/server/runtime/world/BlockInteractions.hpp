#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::world {

/// Opening a door, flipping a lever, pushing a button or a trapdoor: what a hand changes on its own.
///
/// parity: `DoorBlock`, `TrapDoorBlock`, `FenceGateBlock`, `LeverBlock` and `ButtonBlock`'s
/// `useWithoutItem`, which cycle one property and play a sound. What the signal does afterwards is
/// the redstone code's, not this one's.
namespace blockInteractions {

/// parity: the `ButtonBlock` constructors, twenty ticks for every stone-like set and thirty for
/// every wooden one.
inline constexpr std::int32_t kStonePressTicks = 20;
inline constexpr std::int32_t kWoodPressTicks = 30;

/// parity: `RepeaterBlock.DELAY`, one to four.
inline constexpr std::int32_t kMaxDelay = 4;

/// parity: `ComparatorBlock.useWithoutItem`'s own click, quieter than a lever, with the pitch
/// saying which way the mode went.
inline constexpr float kComparatorVolume = 0.3F;
inline constexpr float kSubtractPitch = 0.55F;
inline constexpr float kComparePitch = 0.5F;

/// A block that changed by hand, and the sound it made.
struct BlockToggle {
    std::vector<BlockChange> changes;
    std::string sound;

    friend bool operator==(const BlockToggle&, const BlockToggle&) = default;
};

/// A block whose property moved one notch on, and the sound it made if any.
struct BlockCycle {
    std::int32_t stateId = 0;
    std::optional<std::string> sound;
    float pitch = 1.0F;
    float volume = 1.0F;

    friend bool operator==(const BlockCycle&, const BlockCycle&) = default;
};

/// A block that changed on its own timer, and the sound it made.
struct BlockRelease {
    std::int32_t stateId = 0;
    std::string sound;

    friend bool operator==(const BlockRelease&, const BlockRelease&) = default;
};

/// Whether a hand does anything at all to this block.
[[nodiscard]] bool isToggleable(std::int32_t stateId);

/// Whether a hand moves this block on a notch rather than flipping it.
[[nodiscard]] bool isCycleable(std::int32_t stateId);

/// Flips the block at these coordinates and whatever else goes with it, or nothing when a hand does
/// not move it.
///
/// Both halves of a door move together, which is why this reads the world rather than answering
/// from the state alone.
[[nodiscard]] std::optional<BlockToggle> toggle(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// The right click that moves a repeater's delay on a notch or flips a comparator's mode.
///
/// parity: `RepeaterBlock.useWithoutItem`, which cycles the delay in silence, and
/// `ComparatorBlock.useWithoutItem`, which cycles the mode and clicks.
[[nodiscard]] std::optional<BlockCycle> cycle(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// How long a pressed button stays down, or nothing when the block is not a button.
[[nodiscard]] std::optional<std::int32_t> pressedTicks(std::int32_t stateId);

/// What a button's own scheduled tick does: back up, with the other half of its click.
[[nodiscard]] std::optional<BlockRelease> release(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// The same sound, for the blocks redstone opens rather than a hand: a door, a trapdoor or a fence
/// gate. Anything else answers nothing, because nothing else makes a noise on a signal.
///
/// This is the shape `redstoneReactors::Hooks::openSound` expects.
[[nodiscard]] std::optional<std::string> redstoneSound(std::int32_t stateId, bool open);

} // namespace blockInteractions

} // namespace basalt::server::runtime::world
