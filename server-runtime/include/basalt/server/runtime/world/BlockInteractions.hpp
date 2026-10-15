#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::world {

namespace blockInteractions {

inline constexpr std::int32_t kStonePressTicks = 20;
inline constexpr std::int32_t kWoodPressTicks = 30;

inline constexpr std::int32_t kMaxDelay = 4;

inline constexpr float kComparatorVolume = 0.3F;
inline constexpr float kSubtractPitch = 0.55F;
inline constexpr float kComparePitch = 0.5F;

struct BlockToggle {
    std::vector<BlockChange> changes;
    std::string sound;

    friend bool operator==(const BlockToggle&, const BlockToggle&) = default;
};

struct BlockCycle {
    std::int32_t stateId = 0;
    std::optional<std::string> sound;
    float pitch = 1.0F;
    float volume = 1.0F;

    friend bool operator==(const BlockCycle&, const BlockCycle&) = default;
};

struct BlockRelease {
    std::int32_t stateId = 0;
    std::string sound;

    friend bool operator==(const BlockRelease&, const BlockRelease&) = default;
};

[[nodiscard]] bool isToggleable(std::int32_t stateId);

[[nodiscard]] bool isCycleable(std::int32_t stateId);

[[nodiscard]] bool cycleYieldsToAxe(std::int32_t stateId);

/// parity: only `LeverBlock` and `ButtonBlock` toggle with `UPDATE_NEIGHBORS`.
[[nodiscard]] bool handToggleTellsNeighbours(std::int32_t stateId);

[[nodiscard]] std::optional<BlockToggle> toggle(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                std::optional<float> playerYaw = std::nullopt);

[[nodiscard]] std::optional<BlockCycle> cycle(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] std::optional<std::int32_t> pressedTicks(std::int32_t stateId);

[[nodiscard]] std::optional<BlockRelease> release(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] std::optional<std::string> redstoneSound(std::int32_t stateId, bool open);

[[nodiscard]] std::optional<std::array<std::int32_t, 3>> attachmentOf(std::int32_t stateId, std::int32_t x,
                                                                      std::int32_t y, std::int32_t z);

/// parity: `BellBlock.updateShape`.
[[nodiscard]] std::optional<std::int32_t> bellShape(const WorldState& world, std::int32_t x, std::int32_t y,
                                                    std::int32_t z, std::int32_t stateId,
                                                    server::registry::BlockFace towards);

} // namespace blockInteractions

} // namespace basalt::server::runtime::world
