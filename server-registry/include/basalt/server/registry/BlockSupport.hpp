#pragma once

#include <cstdint>

namespace basalt::server::registry {
// parity: `net.minecraft.world.level.material.PushReaction`.
enum class PushReaction : std::uint8_t { Normal, Destroy, Block, Ignore, PushOnly };

// parity: `BlockBehaviour.canSurvive`, minus the handful of blocks `isModelled` names.
namespace blockSupport {
[[nodiscard]] bool needsSupport(std::int32_t stateId);

[[nodiscard]] bool isModelled(std::int32_t stateId);

[[nodiscard]] bool holdsWater(std::int32_t stateId);

// parity: `BlockState.getPistonPushReaction`.
[[nodiscard]] PushReaction pushReaction(std::int32_t stateId);

// parity: `BlockStateBase.isRedstoneConductor`: whether a signal passes through the block.
[[nodiscard]] bool conductsRedstone(std::int32_t stateId);

// parity: `BlockState.isRandomlyTicking`, which is what makes crops grow and ice melt.
[[nodiscard]] bool ticksRandomly(std::int32_t stateId);

[[nodiscard]] bool canSurvive(std::int32_t stateId, std::int32_t down, std::int32_t up, std::int32_t north,
                              std::int32_t south, std::int32_t west, std::int32_t east, bool wet = false);

[[nodiscard]] std::int32_t blockRegistryIdOf(std::int32_t stateId);
} // namespace blockSupport
} // namespace basalt::server::registry
