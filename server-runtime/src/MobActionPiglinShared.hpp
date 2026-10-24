#pragma once

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/Piglins.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/DoorInteraction.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSpear.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::runtime::mobActionPiglin {

using entity::ServerEntity;
using player::PlayerSession;
using wire::protocol::play::Gamemode;
namespace piglins = entity::piglins;
namespace kit = entity::ai::brainKit;
namespace mth = wire::common::mth;
using piglins::Activity;
using piglins::PiglinState;

struct Sense {
    std::optional<std::int32_t> nemesis;
    std::optional<std::int32_t> huntableHoglin;
    std::optional<std::int32_t> babyHoglin;
    std::optional<std::int32_t> zombified;
    std::optional<std::int32_t> playerNotWearingGold;
    std::optional<std::int32_t> playerHoldingLoved;
    std::optional<std::int32_t> attackablePlayer;
    std::vector<ServerEntity*> adultPiglins;
    std::int32_t visibleAdultPiglins = 0;
    std::int32_t adultHoglins = 0;
    std::optional<piglins::Pos> repellent;
    std::optional<std::int32_t> wantedItem;
};

inline thread_local Sense tSense;

[[nodiscard]] inline std::int32_t sample(random::RandomSource& random, const std::array<std::int32_t, 2>& range) {
    return range[0] + random.nextInt(range[1] - range[0] + 1);
}

[[nodiscard]] inline bool isBaby(const ServerEntity& mob) { return mob.age < 0; }

[[nodiscard]] inline bool isZombified(std::string_view path) { return path == "zombified_piglin" || path == "zoglin"; }

/// parity: `VehicleSystem.sendPassengers`, the packet a mount change announces.
inline void sendPassengers(const ServerEntity& vehicle, const std::vector<PlayerSession*>& watchers) {
    const wire::protocol::play::SetPassengersPacket packet{.vehicleId = vehicle.entityId(),
                                                           .passengerIds = vehicle.passengerIds};
    for (PlayerSession* player : watchers) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::SetPassengersPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

/// parity: `Piglin.startRiding`: a baby stacks on the hoglin's top passenger, three deep.
inline ServerEntity* topPassengerOf(world::LoadedWorld& world, ServerEntity& vehicle, std::int32_t depth) {
    ServerEntity* top = &vehicle;
    for (std::int32_t level = 0; level < depth; ++level) {
        if (top->passengerIds.empty()) {
            break;
        }
        ServerEntity* next = world.state().entity(top->passengerIds.front());
        if (next == nullptr) {
            break;
        }
        top = next;
    }
    return top;
}

inline void settle(kit::Memory<bool>& memory, std::int64_t now) {
    if (memory.until < -1) {
        memory.until = now + (-2 - memory.until);
    }
}

} // namespace basalt::server::runtime::mobActionPiglin
