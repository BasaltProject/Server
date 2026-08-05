#pragma once

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/RedstoneTickSystem.hpp>
#include <basalt/server/runtime/entity/BlockTriggers.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/ArrowButtons.hpp>
#include <basalt/server/runtime/world/Beacons.hpp>
#include <basalt/server/runtime/world/BlockEntityTicks.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/Dripleaves.hpp>
#include <basalt/server/runtime/world/EndGateways.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MiscGrowth.hpp>
#include <basalt/server/runtime/world/Pistons.hpp>
#include <basalt/server/runtime/world/PressurePlates.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <utility>

namespace basalt::server::runtime::redstoneTick {

/// parity: `Entity.getOnPosLegacy`.
constexpr double kOnPosDepth = 0.2;

/// parity: `BlockEventDispatcher`'s reach.
constexpr double kBlockEventRange = 64.0;

using player::PlayerSession;
using wire::protocol::play::BlockPos;
using wire::protocol::play::ClientboundBlockUpdatePacket;
using wire::protocol::play::Gamemode;

inline std::int64_t pack(std::int32_t x, std::int32_t y, std::int32_t z) {
    return ((static_cast<std::int64_t>(x) & 0x3ffffff) << 38) | ((static_cast<std::int64_t>(z) & 0x3ffffff) << 12) |
           (static_cast<std::int64_t>(y) & 0xfff);
}

inline std::int32_t unpackX(std::int64_t packed) { return static_cast<std::int32_t>(packed >> 38); }

inline std::int32_t unpackY(std::int64_t packed) { return static_cast<std::int32_t>((packed << 52) >> 52); }

inline std::int32_t unpackZ(std::int64_t packed) { return static_cast<std::int32_t>((packed << 26) >> 38); }

inline void sendBlockUpdate(const std::vector<PlayerSession*>& watchers, const BlockPos& pos, std::int32_t stateId) {
    for (PlayerSession* player : watchers) {
        player::sendBlockChangeTo(*player, pos.x, pos.y, pos.z, stateId);
    }
}

inline void crossingsOf(world::LoadedWorld& world, double x, double y, double z, double halfWidth, double height,
                        std::set<std::int64_t>& into) {
    for (std::int32_t blockX = wire::common::mth::floor(x - halfWidth);
         blockX <= wire::common::mth::floor(x + halfWidth); ++blockX) {
        for (std::int32_t blockY = wire::common::mth::floor(y); blockY <= wire::common::mth::floor(y + height);
             ++blockY) {
            for (std::int32_t blockZ = wire::common::mth::floor(z - halfWidth);
                 blockZ <= wire::common::mth::floor(z + halfWidth); ++blockZ) {
                const std::optional<std::int32_t> stateId = world.state().blockState(blockX, blockY, blockZ);
                if (!stateId.has_value()) {
                    continue;
                }
                const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(*stateId);
                if (block == nullptr || *block != world::tripwires::wireBlock()) {
                    continue;
                }
                if (world::tripwires::crosses(blockX, blockY, blockZ, *stateId, x, y, z, halfWidth, height)) {
                    into.insert(pack(blockX, blockY, blockZ));
                }
            }
        }
    }
}

} // namespace basalt::server::runtime::redstoneTick
