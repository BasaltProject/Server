#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::world::respawnPoints {

struct Query {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    float yaw = 0.0F;
    bool forced = false;
    bool consumeCharge = true;
};

struct Spot {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;
};

struct Outcome {
    /// Absent when the point is no longer valid.
    std::optional<Spot> spot;
    /// The anchor block after a charge was spent.
    std::optional<BlockChange> spent;
    /// Whether the block at the point is a respawn anchor, valid or not.
    bool anchor = false;
};

/// parity: `ServerPlayer.findRespawnAndUseSpawnBlock`.
[[nodiscard]] Outcome find(LoadedWorld& world, const Query& query, const realBlockTags::TagMembers* tags);

} // namespace basalt::server::runtime::world::respawnPoints
