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

class SculkCatalystSystem {
public:
    struct Options {
        std::function<std::int64_t()> nextSeed;
        const world::realBlockTags::TagMembers* blockTags = nullptr;
    };

    SculkCatalystSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, Options options);

    bool tick(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

    [[nodiscard]] std::optional<std::array<std::int32_t, 3>> nearest(const world::LoadedWorld& world, double x,
                                                                     double y, double z) const;

    void drink(world::LoadedWorld& world, const std::array<std::int32_t, 3>& catalyst, double x, double y, double z,
               std::optional<std::int32_t> experience);

    bool scheduledTick(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

private:
    [[nodiscard]] std::vector<player::PlayerSession*> watchersOf(const world::LoadedWorld& world) const;

    player::PlayerRegistry* players_;
    BlockChangeBroadcaster* broadcaster_;
    Options options_;
};

} // namespace basalt::server::runtime
