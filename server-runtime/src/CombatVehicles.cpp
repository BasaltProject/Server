#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/VehiclePackets.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
namespace play = wire::protocol::play;

/// parity: `ServerGamePacketListenerImpl.clampHorizontal` and `clampVertical`.
constexpr double kHorizontalLimit = 3.0E7;
constexpr double kVerticalLimit = 2.0E7;
/// parity: the hundred squared blocks a vehicle may move in one packet before it is put back.
constexpr double kMovedTooQuickly = 100.0;
/// parity: `vehiclePositionLastResetAt`'s second between two resets of a vehicle someone else drives.
constexpr std::int64_t kResetIntervalTicks = 20;

/// parity: `Mth.wrapDegrees(float)`, Java's remainder then into half a turn either way.
[[nodiscard]] float wrapDegrees(float degrees) {
    float wrapped = std::fmod(degrees, 360.0F);
    if (wrapped >= 180.0F) {
        wrapped -= 360.0F;
    }
    if (wrapped < -180.0F) {
        wrapped += 360.0F;
    }
    return wrapped;
}

[[nodiscard]] bool invalid(double value) { return std::isnan(value) || std::isinf(value); }

[[nodiscard]] bool containsInvalidValues(const play::VehiclePosition& at) {
    return invalid(at.x) || invalid(at.y) || invalid(at.z) || std::isnan(at.yRot) || std::isinf(at.yRot) ||
           std::isnan(at.xRot) || std::isinf(at.xRot);
}

/// parity: `Entity.isEyeInFluid(WATER)` for a boat, its eyes at 85 hundredths of its height.
[[nodiscard]] bool eyeInWater(world::LoadedWorld& world, const ServerEntity& boat) {
    constexpr double kEyeShare = 0.85;
    const std::optional<std::int32_t> state =
        world.state().blockState(static_cast<std::int32_t>(std::floor(boat.x)),
                                 static_cast<std::int32_t>(std::floor(boat.y + (boat.height() * kEyeShare))),
                                 static_cast<std::int32_t>(std::floor(boat.z)));
    return state.has_value() && world::fluids::of(*state) == world::Submersion::Water;
}

/// The top of a block's collision shape, or nothing for an empty one.
[[nodiscard]] std::optional<double> shapeTop(std::int32_t stateId) {
    const std::span<const std::int32_t> boxes = server::registry::blockShapes::collisionBoxes(stateId);
    std::optional<double> top;
    for (std::size_t at = 0; at + server::registry::blockShapes::kCorners <= boxes.size();
         at += server::registry::blockShapes::kCorners) {
        const double maxY = static_cast<double>(boxes[at + 4]) / server::registry::blockShapes::kUnits;
        top = std::max(top.value_or(maxY), maxY);
    }
    return top;
}

/// parity: `BlockGetter.getBlockFloorHeight`: the top of this square's shape, or of the one below
/// less a block when this one is empty, or infinitely far down.
[[nodiscard]] double floorHeight(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (const std::optional<double> top = shapeTop(world.state().blockState(x, y, z).value_or(0)); top.has_value()) {
        return *top;
    }
    const double below = shapeTop(world.state().blockState(x, y - 1, z).value_or(0)).value_or(0.0);
    return below >= 1.0 ? below - 1.0 : -std::numeric_limits<double>::infinity();
}

/// parity: `AbstractBoat.getDismountLocationForPassenger`: the square beside the boat the player
/// faces, on its floor or the one under it, standing, crouching or swimming; else on top of the boat.
[[nodiscard]] std::array<double, 3> boatDismount(world::LoadedWorld& world, const ServerEntity& boat,
                                                 const PlayerSession& player) {
    constexpr double kPlayerWidth = 0.6;
    constexpr std::array<double, 3> kPoseHeights{1.8, 1.5, 0.6};
    constexpr float kRadians = static_cast<float>(std::numbers::pi / 180.0);
    const double distance = (static_cast<double>(static_cast<float>(boat.width()) * std::numbers::sqrt2_v<float>) +
                             kPlayerWidth + static_cast<double>(1.0E-5F)) /
                            2.0;
    const float directionX = -wire::common::mth::sin(player.yaw * kRadians);
    const float directionZ = wire::common::mth::cos(player.yaw * kRadians);
    const float scale = std::max(std::abs(directionX), std::abs(directionZ));
    const double targetX = boat.x + (static_cast<double>(directionX) * distance / static_cast<double>(scale));
    const double targetZ = boat.z + (static_cast<double>(directionZ) * distance / static_cast<double>(scale));
    const auto blockX = static_cast<std::int32_t>(std::floor(targetX));
    const auto blockY = static_cast<std::int32_t>(std::floor(boat.y + boat.height()));
    const auto blockZ = static_cast<std::int32_t>(std::floor(targetZ));
    const std::optional<std::int32_t> below = world.state().blockState(blockX, blockY - 1, blockZ);
    if (!below.has_value() || world::fluids::of(*below) != world::Submersion::Water) {
        std::vector<std::array<double, 3>> targets;
        for (const std::int32_t y : {blockY, blockY - 1}) {
            const double floor = floorHeight(world, blockX, y, blockZ);
            if (!std::isinf(floor) && floor < 1.0) {
                targets.push_back({targetX, static_cast<double>(y) + floor, targetZ});
            }
        }
        const entity::entityCollision::BlockAt blockAt = [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
            return world.state().blockState(x, y, z);
        };
        for (const double height : kPoseHeights) {
            for (const std::array<double, 3>& target : targets) {
                const entity::Aabb box = entity::Aabb::of(target[0], target[1], target[2], kPlayerWidth, height);
                if (!entity::entityCollision::collides(blockAt, box)) {
                    return target;
                }
            }
        }
    }
    return {boat.x, boat.y + boat.height(), boat.z};
}

/// parity: `AbstractHorse.getDismountLocationInDirection`: from the mount's feet up to three
/// quarters of a block over its back, the first floor the player fits on standing, crouching or
/// swimming.
[[nodiscard]] std::optional<std::array<double, 3>> horseDismountToward(world::LoadedWorld& world,
                                                                       const ServerEntity& horse, float degrees) {
    constexpr double kPlayerWidth = 0.6;
    constexpr std::array<double, 3> kPoseHeights{1.8, 1.5, 0.6};
    constexpr float kRadians = static_cast<float>(std::numbers::pi / 180.0);
    // parity: `Entity.getCollisionHorizontalEscapeVector`.
    const double distance = (horse.width() + kPlayerWidth + static_cast<double>(1.0E-5F)) / 2.0;
    const float directionX = -wire::common::mth::sin(degrees * kRadians);
    const float directionZ = wire::common::mth::cos(degrees * kRadians);
    const float scale = std::max(std::abs(directionX), std::abs(directionZ));
    const double targetX = horse.x + (static_cast<double>(directionX) * distance / static_cast<double>(scale));
    const double targetZ = horse.z + (static_cast<double>(directionZ) * distance / static_cast<double>(scale));
    const auto blockX = static_cast<std::int32_t>(std::floor(targetX));
    const auto blockZ = static_cast<std::int32_t>(std::floor(targetZ));
    const double limit = horse.y + horse.height() + 0.75;
    const entity::entityCollision::BlockAt blockAt = [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
        return world.state().blockState(x, y, z);
    };
    for (const double height : kPoseHeights) {
        auto blockY = static_cast<std::int32_t>(std::floor(horse.y));
        while (true) {
            const double floor = floorHeight(world, blockX, blockY, blockZ);
            if (static_cast<double>(blockY) + floor > limit) {
                break;
            }
            if (!std::isinf(floor) && floor < 1.0) {
                const std::array<double, 3> spot{targetX, static_cast<double>(blockY) + floor, targetZ};
                if (!entity::entityCollision::collides(
                        blockAt, entity::Aabb::of(spot[0], spot[1], spot[2], kPlayerWidth, height))) {
                    return spot;
                }
            }
            ++blockY;
            if (static_cast<double>(blockY) >= limit) {
                break;
            }
        }
    }
    return std::nullopt;
}

/// parity: `AbstractHorse.getDismountLocationForPassenger`: the main arm's side, then the other, then
/// where the mount stands. The right arm, which is every player's until the client says otherwise.
[[nodiscard]] std::array<double, 3> horseDismount(world::LoadedWorld& world, const ServerEntity& horse) {
    constexpr float kQuarterTurn = 90.0F;
    if (const auto right = horseDismountToward(world, horse, horse.yaw + kQuarterTurn); right.has_value()) {
        return *right;
    }
    if (const auto left = horseDismountToward(world, horse, horse.yaw - kQuarterTurn); left.has_value()) {
        return *left;
    }
    return {horse.x, horse.y, horse.z};
}

} // namespace

std::optional<std::array<double, 3>> CombatHandler::dismountSpot(const PlayerSession& session,
                                                                 const ServerEntity& vehicle) const {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return std::nullopt;
    }
    if (entity::horses::isHorse(vehicle.type())) {
        return horseDismount(*world, vehicle);
    }
    if (!entity::boats::isBoat(vehicle.type())) {
        return std::nullopt;
    }
    return boatDismount(*world, vehicle, session);
}

bool CombatHandler::controls(const PlayerSession& session, const ServerEntity& vehicle) const {
    // parity: `AbstractBoat.getControllingPassenger`, the first passenger when it is a player, and
    // `AbstractHorse`'s, which also wants a saddle.
    if (vehicle.passengerIds.empty() || vehicle.passengerIds.front() != session.entityId()) {
        return false;
    }
    if (entity::horses::isHorse(vehicle.type())) {
        return entity::horses::saddled(vehicle);
    }
    return entity::boats::isBoat(vehicle.type());
}

bool CombatHandler::boardBoat(PlayerSession& session, ServerEntity& boat) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return false;
    }
    // parity: `AbstractBoat.interact` and `canAddPassenger`: not crouching, a boat still in control,
    // room aboard and its eyes above the water.
    if (session.sneaking || boat.boatOutOfControl >= entity::boats::kOutOfControlTicks) {
        return false;
    }
    if (static_cast<std::int32_t>(boat.passengerIds.size()) >= entity::boats::maxPassengers(boat.type()) ||
        eyeInWater(*world, boat)) {
        return false;
    }
    mount(session, boat);
    return true;
}

void CombatHandler::handleMoveVehicle(const play::ServerboundMoveVehiclePacket& packet) {
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || world == nullptr) {
        return;
    }
    if (containsInvalidValues(packet.movingTo)) {
        owner_->disconnect(wire::text::Component::translatable("multiplayer.disconnect.invalid_vehicle_movement"));
        return;
    }
    ServerEntity* vehicle = session->vehicleId.has_value() ? world->state().entity(*session->vehicleId) : nullptr;
    // parity: `getRootVehicle`, the bottom of the stack.
    while (vehicle != nullptr && vehicle->vehicleId.has_value()) {
        ServerEntity* below = world->state().entity(*vehicle->vehicleId);
        if (below == nullptr) {
            break;
        }
        vehicle = below;
    }
    if (vehicle == nullptr || vehicle->removed) {
        return;
    }
    const auto putBack = [this, vehicle] {
        const play::ClientboundMoveVehiclePacket back{
            .movingTo = {
                .x = vehicle->x, .y = vehicle->y, .z = vehicle->z, .yRot = vehicle->yaw, .xRot = vehicle->pitch}};
        owner_->sendPlayPacket(play::ClientboundMoveVehiclePacket::kId,
                               [&back](wire::io::ByteBuffer& buffer) { back.write(buffer); });
    };
    if (!controls(*session, *vehicle)) {
        // parity: a passenger whose client moves a vehicle it does not drive is told where it is,
        // at most once a second unless it strayed more than a block.
        const double dx = packet.movingTo.x - vehicle->x;
        const double dy = packet.movingTo.y - vehicle->y;
        const double dz = packet.movingTo.z - vehicle->z;
        if (world->gameTime() - vehicleResetAt_ > kResetIntervalTicks || (dx * dx) + (dy * dy) + (dz * dz) > 1.0) {
            putBack();
            vehicleResetAt_ = world->gameTime();
        }
        return;
    }
    const double targetX = std::clamp(packet.movingTo.x, -kHorizontalLimit, kHorizontalLimit);
    const double targetY = std::clamp(packet.movingTo.y, -kVerticalLimit, kVerticalLimit);
    const double targetZ = std::clamp(packet.movingTo.z, -kHorizontalLimit, kHorizontalLimit);
    const double dx = targetX - vehicle->x;
    const double dy = targetY - vehicle->y;
    const double dz = targetZ - vehicle->z;
    const double expected = (vehicle->velocityX * vehicle->velocityX) + (vehicle->velocityY * vehicle->velocityY) +
                            (vehicle->velocityZ * vehicle->velocityZ);
    if ((dx * dx) + (dy * dy) + (dz * dz) - expected > kMovedTooQuickly) {
        owner_->logger().warn(
            [&] { return vehicle->type().toString() + " (vehicle of " + session->name() + ") moved too quickly!"; });
        putBack();
        return;
    }
    vehicle->x = targetX;
    vehicle->y = targetY;
    vehicle->z = targetZ;
    vehicle->yaw = wrapDegrees(packet.movingTo.yRot);
    vehicle->pitch = wrapDegrees(packet.movingTo.xRot);
    vehicle->onGround = packet.onGround;
}

void CombatHandler::handlePaddleBoat(const play::ServerboundPaddleBoatPacket& packet) {
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    ServerEntity* boat = session == nullptr || world == nullptr || !session->vehicleId.has_value()
                             ? nullptr
                             : world->state().entity(*session->vehicleId);
    // parity: `handlePaddleBoat` sets both paddles on whatever boat the player is in.
    if (boat == nullptr || !entity::boats::isBoat(boat->type())) {
        return;
    }
    if (boat->paddleLeft == packet.left && boat->paddleRight == packet.right) {
        return;
    }
    boat->paddleLeft = packet.left;
    boat->paddleRight = packet.right;
    const play::SetEntityDataPacket data{
        .entityId = boat->entityId(),
        .entries = {play::SetEntityDataPacket::Entry{.index = entity::boats::kPaddleLeftDataIndex,
                                                     .value = play::SetEntityDataPacket::BoolValue{packet.left}},
                    play::SetEntityDataPacket::Entry{.index = entity::boats::kPaddleRightDataIndex,
                                                     .value = play::SetEntityDataPacket::BoolValue{packet.right}}}};
    for (PlayerSession* watcher : owner_->playersInSameWorld()) {
        if (watcher->connection().sendPlayPacket) {
            watcher->connection().sendPlayPacket(play::SetEntityDataPacket::kId,
                                                 [&data](wire::io::ByteBuffer& buffer) { data.write(buffer); });
        }
    }
}

} // namespace basalt::server::runtime
