#include "SessionPlayShared.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime {
namespace {

constexpr double kFloatingDrop = -0.03125;
constexpr std::int32_t kBaseFloatingTicks = 80;
constexpr double kDefaultGravity = 0.08;
constexpr double kSlowFallingGravity = 0.01;

[[nodiscard]] double heightOf(wire::protocol::play::Pose pose) {
    using Pose = wire::protocol::play::Pose;
    switch (pose) {
    case Pose::FallFlying:
    case Pose::Swimming:
    case Pose::SpinAttack:
        return 0.6;
    case Pose::Crouching:
        return 1.5;
    case Pose::Sleeping:
    case Pose::Dying:
        return 0.2;
    default:
        return 1.8;
    }
}

} // namespace

// parity: `ServerGamePacketListenerImpl.noBlocksAround`.
bool ClientSession::noBlocksAround(const player::PlayerSession& session) {
    constexpr double kHalfWidth = 0.3;
    constexpr double kInflate = 0.0625;
    constexpr double kReach = 0.55;
    const auto floorOf = [](double value) { return static_cast<std::int32_t>(std::floor(value)); };
    const std::int32_t minX = floorOf(session.x - kHalfWidth - kInflate);
    const std::int32_t maxX = floorOf(session.x + kHalfWidth + kInflate);
    const std::int32_t minY = floorOf(session.y - kInflate - kReach);
    const std::int32_t maxY = floorOf(session.y + heightOf(session.pose) + kInflate);
    const std::int32_t minZ = floorOf(session.z - kHalfWidth - kInflate);
    const std::int32_t maxZ = floorOf(session.z + kHalfWidth + kInflate);
    for (std::int32_t x = minX; x <= maxX; ++x) {
        for (std::int32_t y = minY; y <= maxY; ++y) {
            for (std::int32_t z = minZ; z <= maxZ; ++z) {
                const std::optional<std::int32_t> state = world().blockState(x, y, z);
                if (state.has_value() && !server::registry::blocks::isAir(*state)) {
                    return false;
                }
            }
        }
    }
    return true;
}

// parity: `handlePlayerPositionChange` clientIsFloating; the block under the feet stands for verticalCollisionBelow.
void ClientSession::noteFloating(double dy) {
    const player::PlayerSession* session = playerSession_.get();
    clientFloating_ = session != nullptr && dy >= kFloatingDrop && !options_.tuning.allowFlight &&
                      session->gamemode != wire::protocol::play::Gamemode::Spectator && !session->mayFly() &&
                      !session->effects.contains(wire::common::Identifier::vanilla("levitation")) &&
                      !session->fallFlying && session->autoSpinTicks <= 0 && noBlocksAround(*session);
}

// parity: `ServerGamePacketListenerImpl.tickPlayer`, `multiplayer.disconnect.flying`.
void ClientSession::kickIfFloating() {
    const player::PlayerSession* session = playerSession_.get();
    if (session == nullptr || closeWhenDrained_.load() || !clientFloating_ || session->sleepingBed.has_value() ||
        session->vehicleId.has_value() || session->dead) {
        clientFloating_ = false;
        floatingTicks_ = 0;
        return;
    }
    const double gravity = session->effects.contains(wire::common::Identifier::vanilla("slow_falling"))
                               ? kSlowFallingGravity
                               : kDefaultGravity;
    const auto limit =
        static_cast<std::int32_t>(std::ceil(kBaseFloatingTicks * std::max(kDefaultGravity / gravity, 1.0)));
    if (++floatingTicks_ > limit) {
        logger_.warn([session] { return session->name() + " was kicked for floating too long!"; });
        disconnect(messageFor("flying", wire::text::Component::translatable("multiplayer.disconnect.flying")));
    }
}

} // namespace basalt::server::runtime
