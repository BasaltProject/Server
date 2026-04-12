#pragma once

#include <basalt/server/runtime/world/RealBlockTags.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime {

class BlockChangeBroadcaster;

/// The sculk catalyst's turn in the world: the deaths it drinks, its bloom and its spreader.
///
/// parity: `SculkCatalystBlock`, `SculkCatalystBlockEntity` and its `CatalystListener`.
class SculkCatalystSystem {
public:
    struct Options {
        std::function<std::int64_t()> nextSeed;
        const world::realBlockTags::TagMembers* blockTags = nullptr;
    };

    SculkCatalystSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, Options options);

    /// parity: `SculkCatalystBlockEntity.serverTick`. Answers whether its cursors changed.
    bool tick(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

    /// The nearest catalyst in reach of a death, if any. parity: the `BY_DISTANCE` delivery of
    /// `ENTITY_DIE`, which only the first listener finds unconsumed.
    [[nodiscard]] std::optional<std::array<std::int32_t, 3>> nearest(const world::LoadedWorld& world, double x,
                                                                     double y, double z) const;

    /// parity: `CatalystListener.handleGameEvent`: the dead's experience turned into charge where it
    /// fell, and the bloom. `experience` is what it would have dropped, nothing when it drops none.
    void drink(world::LoadedWorld& world, const std::array<std::int32_t, 3>& catalyst, double x, double y, double z,
               std::optional<std::int32_t> experience);

    /// parity: `SculkCatalystBlock.tick`, the bloom let go of. Answers whether it changed.
    bool scheduledTick(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

private:
    [[nodiscard]] std::vector<player::PlayerSession*> watchersOf(const world::LoadedWorld& world) const;

    player::PlayerRegistry* players_;
    BlockChangeBroadcaster* broadcaster_;
    Options options_;
};

} // namespace basalt::server::runtime
