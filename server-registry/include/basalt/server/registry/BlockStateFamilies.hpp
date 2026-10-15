#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>

namespace basalt::server::registry::blockStateFamilies {
[[nodiscard]] bool isLeavesState(std::int32_t stateId);

[[nodiscard]] std::optional<std::int32_t> leafDistance(std::int32_t stateId);

[[nodiscard]] std::optional<std::int32_t> withLeafDistance(std::int32_t stateId, std::int32_t distance);

[[nodiscard]] bool isDoublePlantState(std::int32_t stateId);

[[nodiscard]] std::optional<std::int32_t> upperDoublePlantStateId(std::int32_t lowerStateId);

[[nodiscard]] std::optional<std::int32_t> withSnowy(std::int32_t stateId, bool snowy);

[[nodiscard]] bool isKelpState(std::int32_t stateId);

[[nodiscard]] std::optional<std::int32_t> growingVineHeadStateId(std::int32_t stateId, std::int32_t age);

[[nodiscard]] bool isSeaPickleState(std::int32_t stateId);

[[nodiscard]] bool isCoralPlantState(std::int32_t stateId);

[[nodiscard]] std::optional<std::int32_t> coralWallFanStateId(std::int32_t stateId, std::string_view facing,
                                                              bool waterlogged);

[[nodiscard]] bool isHugeMushroomState(std::int32_t stateId);

[[nodiscard]] std::optional<std::int32_t> hugeMushroomStateId(std::int32_t stateId, bool down, bool east, bool north,
                                                              bool south, bool up, bool west);

[[nodiscard]] bool isWildflowersState(std::int32_t stateId);
[[nodiscard]] bool isPinkPetalsState(std::int32_t stateId);
[[nodiscard]] bool isGlowLichenState(std::int32_t stateId);
[[nodiscard]] bool isVineState(std::int32_t stateId);
[[nodiscard]] bool isCocoaState(std::int32_t stateId);
[[nodiscard]] bool isPaleHangingMossState(std::int32_t stateId);
[[nodiscard]] bool isCreakingHeartState(std::int32_t stateId);
[[nodiscard]] bool isMangrovePropaguleState(std::int32_t stateId);
[[nodiscard]] bool isLeafLitterState(std::int32_t stateId);
[[nodiscard]] bool isBeeNestState(std::int32_t stateId);

[[nodiscard]] std::int32_t chorusPlantStateId(bool down, bool east, bool north, bool south, bool up, bool west);

[[nodiscard]] std::int32_t chorusFlowerStateId(std::int32_t age);

[[nodiscard]] std::int32_t ironBarsStateId(bool north, bool east, bool south, bool west);
} // namespace basalt::server::registry::blockStateFamilies
