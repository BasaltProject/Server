#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `Level.getBestNeighborSignal`.
namespace redstone {

inline constexpr std::int32_t kMaxSignal = 15;

inline constexpr std::string_view kComparatorOutputKey = "OutputSignal";

/// parity: `BlockState.getSignal`.
[[nodiscard]] std::int32_t signalOf(std::int32_t stateId, registry::BlockFace from, std::int32_t openers = 0,
                                    std::int32_t comparatorOutput = kMaxSignal, bool mutedWire = false,
                                    bool playing = true, bool testPowered = false);

/// parity: `getDirectSignal`.
[[nodiscard]] std::int32_t directSignalOf(std::int32_t stateId, registry::BlockFace from, std::int32_t openers = 0,
                                          std::int32_t comparatorOutput = kMaxSignal, bool mutedWire = false);

/// parity: `SignalGetter.getSignal`.
[[nodiscard]] std::int32_t signalFrom(const WorldState& world, std::string_view worldName, std::int32_t x,
                                      std::int32_t y, std::int32_t z, registry::BlockFace from, bool mutedWire = false);

/// Fifteen when nothing is stored.
[[nodiscard]] std::int32_t comparatorOutputAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `SignalGetter.getDirectSignalTo`.
[[nodiscard]] std::int32_t directSignalTo(const WorldState& world, std::string_view worldName, std::int32_t x,
                                          std::int32_t y, std::int32_t z, bool mutedWire = false);

/// parity: `getRedstoneSignalFromContainer`.
[[nodiscard]] std::int32_t containerSignal(std::span<const std::pair<std::int32_t, std::int32_t>> slots,
                                           std::int32_t size);

/// parity: `Level.getBestNeighborSignal`: the strongest of the six faces around a block.
[[nodiscard]] std::int32_t bestNeighbourSignal(const WorldState& world, std::string_view worldName, std::int32_t x,
                                               std::int32_t y, std::int32_t z);

/// parity: `calculateTargetStrength`.
[[nodiscard]] std::int32_t wireStrength(const WorldState& world, std::string_view worldName, std::int32_t x,
                                        std::int32_t y, std::int32_t z);

[[nodiscard]] bool isWire(std::int32_t stateId);

/// parity: `BlockBehaviour.isSignalSource`, the blocks that can feed a circuit.
[[nodiscard]] bool isSignalSource(std::int32_t stateId);

/// parity: `LightningRodBlock.updateNeighbours`.
[[nodiscard]] std::vector<registry::BlockFace> alsoTells(std::int32_t stateId);

/// parity: `updateNeighborsAt`.
[[nodiscard]] std::vector<std::array<std::int32_t, 3>> reachOf(const WorldState& world, std::int32_t x, std::int32_t y,
                                                               std::int32_t z);

using Notify = std::function<void(std::int32_t x, std::int32_t y, std::int32_t z)>;

/// parity: `RedStoneWireBlock.onPlace` asks it.
enum class Asked : std::uint8_t { WithSelf, NeighboursOnly };

std::vector<BlockChange> settle(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                std::int32_t z, const Notify& notify = {}, Asked asked = Asked::WithSelf);

} // namespace redstone

} // namespace basalt::server::runtime::world
