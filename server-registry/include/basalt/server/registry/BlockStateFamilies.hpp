#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>

namespace basalt::server::registry::blockStateFamilies {

/// The state-id arithmetic behind the block families worldgen builds with.
///
/// Vanilla reaches these through property maps; here they are offsets from a family's default
/// state, because the generated tables lay every family out contiguously and a property lookup per
/// block placed is the difference between a chunk generating and a chunk crawling.

/// Whether the id is one of the 28 distance, persistence and water states of a leaves block.
[[nodiscard]] bool isLeavesState(std::int32_t stateId);

/// The vanilla distance property of a leaves state, empty for any other block.
[[nodiscard]] std::optional<std::int32_t> leafDistance(std::int32_t stateId);

/// Changes only the distance property of a leaves state.
[[nodiscard]] std::optional<std::int32_t> withLeafDistance(std::int32_t stateId, std::int32_t distance);

/// Whether the id is an upper or lower half of a worldgen double plant.
[[nodiscard]] bool isDoublePlantState(std::int32_t stateId);

/// The upper-half state matching a lower double-plant state.
[[nodiscard]] std::optional<std::int32_t> upperDoublePlantStateId(std::int32_t lowerStateId);

/// The matching snowy or bare state of grass block, podzol or mycelium.
[[nodiscard]] std::optional<std::int32_t> withSnowy(std::int32_t stateId, bool snowy);

/// Whether the id is one of kelp's 26 age states.
[[nodiscard]] bool isKelpState(std::int32_t stateId);

/// A twisting or weeping vine head state at the requested age.
[[nodiscard]] std::optional<std::int32_t> growingVineHeadStateId(std::int32_t stateId, std::int32_t age);

/// Whether the id is one of sea pickle's count and water states.
[[nodiscard]] bool isSeaPickleState(std::int32_t stateId);

/// Whether the id is a live coral plant, floor fan or wall fan state.
[[nodiscard]] bool isCoralPlantState(std::int32_t stateId);

/// A wall coral fan state with the requested horizontal facing and water state.
[[nodiscard]] std::optional<std::int32_t> coralWallFanStateId(std::int32_t stateId, std::string_view facing,
                                                              bool waterlogged);

/// Whether the id is one of a huge mushroom cap or stem block's 64 face states.
[[nodiscard]] bool isHugeMushroomState(std::int32_t stateId);

/// A huge mushroom block state with the requested visible faces.
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

/// A chorus plant state with the six exact vanilla connection properties.
[[nodiscard]] std::int32_t chorusPlantStateId(bool down, bool east, bool north, bool south, bool up, bool west);

/// The chorus flower state for vanilla age 0 through 5.
[[nodiscard]] std::int32_t chorusFlowerStateId(std::int32_t age);

/// A dry iron-bars state with the exact vanilla horizontal connections.
[[nodiscard]] std::int32_t ironBarsStateId(bool north, bool east, bool south, bool west);

} // namespace basalt::server::registry::blockStateFamilies
