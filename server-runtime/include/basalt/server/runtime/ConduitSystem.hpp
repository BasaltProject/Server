#pragma once

#include <basalt/server/runtime/world/RealBlockTags.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace basalt::wire::nbt {
class NbtCompound;
}

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime {

class BlockChangeBroadcaster;

/// The conduit's turn: its frame, the power it hands out and the monsters it strikes.
///
/// parity: `ConduitBlockEntity.serverTick`.
class ConduitSystem {
public:
    struct Options {
        std::function<std::int64_t()> nextSeed;
        const world::realBlockTags::TagMembers* blockTags = nullptr;
    };

    ConduitSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, Options options);

    /// One conduit's turn over its saved fields. Answers whether they changed.
    bool tick(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z, wire::nbt::NbtCompound& data);

private:
    /// What the jar keeps on the block entity and never saves.
    struct Memory {
        bool active = false;
        std::int64_t nextAmbient = 0;
        std::int64_t lastTick = -2;
    };

    using Key = std::tuple<std::string, std::int32_t, std::int32_t, std::int32_t>;

    [[nodiscard]] std::vector<player::PlayerSession*> watchersOf(const world::LoadedWorld& world) const;

    player::PlayerRegistry* players_;
    BlockChangeBroadcaster* broadcaster_;
    Options options_;
    std::map<Key, Memory> memory_;
};

} // namespace basalt::server::runtime
