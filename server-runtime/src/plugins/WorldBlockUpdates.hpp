#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace basalt::server::runtime {
class BlockChangeBroadcaster;

namespace player {
class PlayerSession;
} // namespace player

namespace world {
class LoadedWorld;
} // namespace world
} // namespace basalt::server::runtime

namespace basalt::server::runtime::plugins::blockUpdates {

struct Target {
    world::LoadedWorld& world;
    BlockChangeBroadcaster* blocks = nullptr;
    std::vector<player::PlayerSession*> watchers;
};

/// Runs the `PluginWorldRef::BlockEffects` in `effects` for blocks already written, `previous` being what each held.
std::int32_t settle(const Target& target, std::span<const std::int32_t> positions,
                    std::span<const std::int32_t> previous, std::uint32_t effects);

/// parity: `BlockState.handleNeighborChanged` on each position, told by the states in `sources` (-1 for unknown).
std::int32_t neighbourChanged(const Target& target, std::span<const std::int32_t> positions,
                              std::span<const std::int32_t> sources);

/// parity: `Block.updateFromNeighbourShapes`.
std::int32_t validState(const Target& target, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

/// parity: `updateNeighbourShapes` for each position, or `updateIndirectNeighbourShapes` for the given states.
std::int32_t shapes(const Target& target, std::span<const std::int32_t> positions, std::span<const std::int32_t> states,
                    bool indirect);

} // namespace basalt::server::runtime::plugins::blockUpdates
