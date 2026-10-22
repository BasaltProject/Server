#include "WorldBlockUpdates.hpp"

#include <basalt/server/plugins/PluginWorldAccess.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>

#include <optional>

namespace basalt::server::runtime::plugins::blockUpdates {
namespace {

namespace phase = world::neighbourUpdates::phase;
using Effects = server::plugins::PluginWorldRef::BlockEffects;

[[nodiscard]] std::uint32_t phasesOf(const std::uint32_t effects) {
    std::uint32_t phases = 0;
    phases |= (effects & Effects::kDeparture) != 0 ? phase::kDeparture : 0;
    phases |= (effects & Effects::kPlace) != 0 ? phase::kPlace : 0;
    phases |= (effects & Effects::kTicks) != 0 ? phase::kTicks : 0;
    phases |= (effects & Effects::kShapes) != 0 ? phase::kShapes : 0;
    phases |= (effects & Effects::kNeighbors) != 0 ? phase::kNeighbours : 0;
    return phases;
}

[[nodiscard]] bool loaded(const Target& target, const std::int32_t x, const std::int32_t z) {
    return target.world.state().isLoaded(x >> 4, z >> 4);
}

[[nodiscard]] std::optional<std::int32_t> known(const std::span<const std::int32_t> states, const std::size_t index) {
    return index < states.size() && states[index] >= 0 ? std::optional{states[index]} : std::nullopt;
}

} // namespace

std::int32_t settle(const Target& target, const std::span<const std::int32_t> positions,
                    const std::span<const std::int32_t> previous, const std::uint32_t effects) {
    const std::uint32_t phases = phasesOf(effects);
    std::int32_t reached = 0;
    for (std::size_t at = 0; at + 2 < positions.size(); at += 3) {
        const std::int32_t x = positions[at];
        const std::int32_t y = positions[at + 1];
        const std::int32_t z = positions[at + 2];
        if (!loaded(target, x, z)) {
            continue;
        }
        ++reached;
        if (phases != 0 && target.blocks != nullptr) {
            target.blocks->settleBlock(target.world, target.watchers, x, y, z, known(previous, at / 3), phases);
        }
    }
    return reached;
}

std::int32_t neighbourChanged(const Target& target, const std::span<const std::int32_t> positions,
                              const std::span<const std::int32_t> sources) {
    std::int32_t reached = 0;
    for (std::size_t at = 0; at + 2 < positions.size(); at += 3) {
        if (!loaded(target, positions[at], positions[at + 2])) {
            continue;
        }
        ++reached;
        if (target.blocks != nullptr) {
            target.blocks->neighbourChanged(target.world, target.watchers, positions[at], positions[at + 1],
                                            positions[at + 2], known(sources, at / 3));
        }
    }
    return reached;
}

std::int32_t shapes(const Target& target, const std::span<const std::int32_t> positions,
                    const std::span<const std::int32_t> states, const bool indirect) {
    std::int32_t reached = 0;
    for (std::size_t at = 0; at + 2 < positions.size(); at += 3) {
        const std::int32_t x = positions[at];
        const std::int32_t y = positions[at + 1];
        const std::int32_t z = positions[at + 2];
        if (!loaded(target, x, z)) {
            continue;
        }
        ++reached;
        if (target.blocks == nullptr) {
            continue;
        }
        if (indirect) {
            if (const std::optional<std::int32_t> state = known(states, at / 3); state.has_value()) {
                target.blocks->reshapeIndirect(target.world, target.watchers, x, y, z, *state);
            }
        } else {
            target.blocks->reshapeAround(target.world, target.watchers, x, y, z);
        }
    }
    return reached;
}

} // namespace basalt::server::runtime::plugins::blockUpdates
