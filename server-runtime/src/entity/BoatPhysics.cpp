#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace basalt::server::runtime::entity::boats {
namespace {

/// parity: `AbstractBoat.getDefaultGravity`.
constexpr double kGravity = 0.04;

[[nodiscard]] std::int32_t floorOf(double value) { return static_cast<std::int32_t>(std::floor(value)); }

[[nodiscard]] std::int32_t ceilOf(double value) { return static_cast<std::int32_t>(std::ceil(value)); }

[[nodiscard]] Aabb boxOf(const ServerEntity& boat) {
    return Aabb::of(boat.x, boat.y, boat.z, boat.width(), boat.height());
}

/// The water a square holds, and how high it stands, or nothing for no water.
struct Water {
    float height = 0.0F;
    bool source = false;
};

[[nodiscard]] std::optional<Water> waterAt(const world::WorldState& world, std::int32_t x, std::int32_t y,
                                           std::int32_t z) {
    const std::int32_t state = world.blockState(x, y, z).value_or(0);
    if (world::fluidFlow::kindOf(state) != world::FluidKind::Water) {
        return std::nullopt;
    }
    return Water{.height = world::fluidFlow::heightAt(world, x, y, z), .source = world::isSourceFluid(state)};
}

/// parity: `isUnderwater`: water standing above the top of the box.
[[nodiscard]] std::optional<Status> underwater(const world::WorldState& world, const Aabb& box) {
    const double maxY = box.maxY + 0.001;
    bool under = false;
    for (std::int32_t x = floorOf(box.minX); x < ceilOf(box.maxX); ++x) {
        for (std::int32_t y = floorOf(box.maxY); y < ceilOf(maxY); ++y) {
            for (std::int32_t z = floorOf(box.minZ); z < ceilOf(box.maxZ); ++z) {
                const std::optional<Water> water = waterAt(world, x, y, z);
                if (water.has_value() && maxY < static_cast<double>(static_cast<float>(y) + water->height)) {
                    if (!water->source) {
                        return Status::UnderFlowingWater;
                    }
                    under = true;
                }
            }
        }
    }
    return under ? std::optional{Status::UnderWater} : std::nullopt;
}

/// parity: `checkInWater`: water around the bottom of the box, and the highest surface of it.
[[nodiscard]] bool inWater(const world::WorldState& world, const Aabb& box, double& waterLevel) {
    bool wet = false;
    waterLevel = -std::numeric_limits<double>::max();
    for (std::int32_t x = floorOf(box.minX); x < ceilOf(box.maxX); ++x) {
        for (std::int32_t y = floorOf(box.minY); y < ceilOf(box.minY + 0.001); ++y) {
            for (std::int32_t z = floorOf(box.minZ); z < ceilOf(box.maxZ); ++z) {
                const std::optional<Water> water = waterAt(world, x, y, z);
                if (!water.has_value()) {
                    continue;
                }
                const float surface = static_cast<float>(y) + water->height;
                waterLevel = std::max(static_cast<double>(surface), waterLevel);
                wet = wet || box.minY < static_cast<double>(surface);
            }
        }
    }
    return wet;
}

/// Whether any collision box of this square overlaps `box` with some volume. parity:
/// `Shapes.joinIsNotEmpty(..., BooleanOp.AND)`.
[[nodiscard]] bool overlaps(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z, const Aabb& box) {
    const std::span<const std::int32_t> boxes = registry::blockShapes::collisionBoxes(stateId);
    constexpr double kUnit = 1.0 / registry::blockShapes::kUnits;
    for (std::size_t at = 0; at + registry::blockShapes::kCorners <= boxes.size();
         at += registry::blockShapes::kCorners) {
        const double minX = x + (boxes[at] * kUnit);
        const double minY = y + (boxes[at + 1] * kUnit);
        const double minZ = z + (boxes[at + 2] * kUnit);
        const double maxX = x + (boxes[at + 3] * kUnit);
        const double maxY = y + (boxes[at + 4] * kUnit);
        const double maxZ = z + (boxes[at + 5] * kUnit);
        if (minX < box.maxX && maxX > box.minX && minY < box.maxY && maxY > box.minY && minZ < box.maxZ &&
            maxZ > box.minZ) {
            return true;
        }
    }
    return false;
}

/// parity: `getGroundFriction`: the mean slipperiness of what the bottom of the boat rests on,
/// leaving the corner columns and lily pads out.
[[nodiscard]] float groundFriction(const world::WorldState& world, const Aabb& whole) {
    const Aabb box{whole.minX, whole.minY - 0.001, whole.minZ, whole.maxX, whole.minY, whole.maxZ};
    const std::int32_t x0 = floorOf(box.minX) - 1;
    const std::int32_t x1 = ceilOf(box.maxX) + 1;
    const std::int32_t y0 = floorOf(box.minY) - 1;
    const std::int32_t y1 = ceilOf(box.maxY) + 1;
    const std::int32_t z0 = floorOf(box.minZ) - 1;
    const std::int32_t z1 = ceilOf(box.maxZ) + 1;
    float friction = 0.0F;
    std::int32_t count = 0;
    for (std::int32_t x = x0; x < x1; ++x) {
        for (std::int32_t z = z0; z < z1; ++z) {
            const std::int32_t edges = (x != x0 && x != x1 - 1 ? 0 : 1) + (z != z0 && z != z1 - 1 ? 0 : 1);
            if (edges == 2) {
                continue;
            }
            for (std::int32_t y = y0; y < y1; ++y) {
                if (edges > 0 && (y == y0 || y == y1 - 1)) {
                    continue;
                }
                const std::int32_t state = world.blockState(x, y, z).value_or(0);
                const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(state);
                if ((block == nullptr || block->path() != "lily_pad") && overlaps(state, x, y, z, box)) {
                    friction += registry::blockMovement::frictionOf(state);
                    ++count;
                }
            }
        }
    }
    return friction / static_cast<float>(count);
}

/// parity: `getWaterLevelAbove`: the first layer above the boat that is not water all the way up.
[[nodiscard]] float waterLevelAbove(const world::WorldState& world, const Aabb& box, double lastYd) {
    const std::int32_t minX = floorOf(box.minX);
    const std::int32_t maxX = ceilOf(box.maxX);
    const std::int32_t minY = floorOf(box.maxY);
    const std::int32_t maxY = ceilOf(box.maxY - lastYd);
    const std::int32_t minZ = floorOf(box.minZ);
    const std::int32_t maxZ = ceilOf(box.maxZ);
    for (std::int32_t y = minY; y < maxY; ++y) {
        float blockHeight = 0.0F;
        bool full = false;
        for (std::int32_t x = minX; x < maxX && !full; ++x) {
            for (std::int32_t z = minZ; z < maxZ && !full; ++z) {
                if (const std::optional<Water> water = waterAt(world, x, y, z); water.has_value()) {
                    blockHeight = std::max(blockHeight, water->height);
                }
                full = blockHeight >= 1.0F;
            }
        }
        if (!full) {
            return static_cast<float>(y) + blockHeight;
        }
    }
    return static_cast<float>(maxY + 1);
}

[[nodiscard]] Status statusOf(const world::WorldState& world, const ServerEntity& boat, BoatState& state) {
    const Aabb box = boxOf(boat);
    if (const std::optional<Status> under = underwater(world, box); under.has_value()) {
        state.waterLevel = box.maxY;
        return *under;
    }
    if (inWater(world, box, state.waterLevel)) {
        return Status::InWater;
    }
    const float friction = groundFriction(world, box);
    if (friction > 0.0F) {
        state.landFriction = friction;
        return Status::OnLand;
    }
    return Status::InAir;
}

/// parity: `floatBoat`.
void floatBoat(world::WorldState& world, ServerEntity& boat, BoatState& state, bool drivenByPlayer) {
    double vspeed = -kGravity;
    double buoyancy = 0.0;
    float invFriction = 0.05F;
    if (state.oldStatus == Status::InAir && state.status != Status::InAir && state.status != Status::OnLand) {
        state.waterLevel = boat.y + boat.height();
        const double targetY =
            static_cast<double>(waterLevelAbove(world, boxOf(boat), state.lastYd)) - boat.height() + 0.101;
        const entityCollision::BlockAt blockAt = [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
            return world.blockState(x, y, z);
        };
        if (!entityCollision::collides(blockAt, Aabb::of(boat.x, targetY, boat.z, boat.width(), boat.height()))) {
            boat.y = targetY;
            boat.velocityY = 0.0;
            state.lastYd = 0.0;
        }
        state.status = Status::InWater;
        return;
    }
    if (state.status == Status::InWater) {
        buoyancy = (state.waterLevel - boat.y) / boat.height();
        invFriction = 0.9F;
    } else if (state.status == Status::UnderFlowingWater) {
        vspeed = -7.0E-4;
        invFriction = 0.9F;
    } else if (state.status == Status::UnderWater) {
        buoyancy = static_cast<double>(0.01F);
        invFriction = 0.45F;
    } else if (state.status == Status::InAir) {
        invFriction = 0.9F;
    } else if (state.status == Status::OnLand) {
        invFriction = state.landFriction;
        if (drivenByPlayer) {
            state.landFriction /= 2.0F;
        }
    }
    boat.velocityX *= static_cast<double>(invFriction);
    boat.velocityY += vspeed;
    boat.velocityZ *= static_cast<double>(invFriction);
    if (buoyancy > 0.0) {
        boat.velocityY = (boat.velocityY + (buoyancy * (kGravity / 0.65))) * static_cast<double>(0.75F);
    }
}

/// parity: `Entity.move(SELF)` for a boat: blocks stop it axis by axis and a stopped axis loses its
/// speed.
void move(world::WorldState& world, ServerEntity& boat, BoatState& state) {
    const entityCollision::BlockAt blockAt = [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
        return world.blockState(x, y, z);
    };
    const std::array<double, 3> moved =
        entityCollision::collide(blockAt, boxOf(boat), boat.velocityX, boat.velocityY, boat.velocityZ);
    const double ya = moved[1];
    boat.x += moved[0];
    boat.y += moved[1];
    boat.z += moved[2];
    boat.onGround = moved[1] != boat.velocityY && boat.velocityY < 0.0;
    if (moved[0] != boat.velocityX) {
        boat.velocityX = 0.0;
    }
    if (moved[2] != boat.velocityZ) {
        boat.velocityZ = 0.0;
    }
    if (moved[1] != boat.velocityY) {
        boat.velocityY = 0.0;
    }
    // parity: `AbstractBoat.checkFallDamage` keeps the drop the float reads next time.
    state.lastYd = ya;
}

} // namespace

double rideHeight(const ServerEntity& boat) {
    const auto height = static_cast<float>(boat.height());
    return boat.type().path().ends_with("_raft") ? static_cast<double>(height * 0.8888889F)
                                                 : static_cast<double>(height / 3.0F);
}

TickResult tick(world::WorldState& world, ServerEntity& boat, bool drivenByPlayer,
                const std::vector<ServerEntity*>& living, const CannotBoard& cannotBoard) {
    TickResult result;
    if (boat.boat == nullptr) {
        boat.boat = std::make_shared<BoatState>();
    }
    BoatState& state = *boat.boat;
    state.oldStatus = state.status;
    state.status = statusOf(world, boat, state);
    if (state.status != Status::UnderWater && state.status != Status::UnderFlowingWater) {
        boat.boatOutOfControl = 0.0F;
    } else {
        boat.boatOutOfControl += 1.0F;
    }
    result.ejectAll = boat.boatOutOfControl >= kOutOfControlTicks && !boat.passengerIds.empty();
    // parity: `VehicleEntity`'s damage wears off a point a turn.
    if (boat.vehicleDamage > 0.0F) {
        boat.vehicleDamage -= 1.0F;
    }
    if (drivenByPlayer) {
        boat.velocityX = 0.0;
        boat.velocityY = 0.0;
        boat.velocityZ = 0.0;
    } else {
        floatBoat(world, boat, state, drivenByPlayer);
        move(world, boat, state);
    }
    // parity: the oars: a stroke splashes as it passes an eighth of a turn, by what the boat is on,
    // and a boat nobody drives keeps its oars still.
    const auto yawRadians = boat.yaw * static_cast<float>(std::numbers::pi / 180.0);
    const std::array<bool, 2> paddles{boat.paddleLeft && drivenByPlayer, boat.paddleRight && drivenByPlayer};
    for (std::size_t side = 0; side < 2; ++side) {
        if (!paddles[side]) {
            state.paddlePositions[side] = 0.0F;
            continue;
        }
        constexpr auto kTurn = static_cast<float>(std::numbers::pi * 2.0);
        constexpr auto kEighth = static_cast<float>(std::numbers::pi / 4.0);
        constexpr auto kStep = static_cast<float>(std::numbers::pi / 8.0);
        const float position = state.paddlePositions[side];
        if (std::fmod(position, kTurn) <= kEighth && std::fmod(position + kStep, kTurn) >= kEighth) {
            const std::string_view sound = state.status == Status::OnLand  ? "entity.boat.paddle_land"
                                           : state.status == Status::InAir ? std::string_view{}
                                                                           : "entity.boat.paddle_water";
            if (!sound.empty()) {
                // parity: `getViewVector` from the pitch and yaw, one block to the oar's side.
                const auto pitchRadians = boat.pitch * static_cast<float>(std::numbers::pi / 180.0);
                const double viewX =
                    static_cast<double>(-wire::common::mth::sin(yawRadians) * wire::common::mth::cos(pitchRadians));
                const double viewZ =
                    static_cast<double>(wire::common::mth::cos(yawRadians) * wire::common::mth::cos(pitchRadians));
                const double dx = side == 1 ? -viewZ : viewZ;
                const double dz = side == 1 ? viewX : -viewX;
                result.strokes.push_back({.sound = sound, .x = boat.x + dx, .y = boat.y, .z = boat.z + dz});
            }
        }
        state.paddlePositions[side] = position + kStep;
    }
    // parity: the entities in the box grown by a fifth sideways, which a boat nobody drives takes
    // aboard while it has room: living, narrower than the boat and not in the tag.
    const Aabb box = boxOf(boat);
    const Aabb reach{box.minX - 0.2, box.minY + 0.01, box.minZ - 0.2, box.maxX + 0.2, box.maxY - 0.01, box.maxZ + 0.2};
    for (ServerEntity* other : living) {
        if (drivenByPlayer || other == &boat || other->removed || other->dying || other->vehicleId.has_value() ||
            static_cast<std::int32_t>(boat.passengerIds.size()) >= maxPassengers(boat.type())) {
            continue;
        }
        if (!registry::entityAttributes::isLiving(other->type()) || other->width() >= boat.width() ||
            (cannotBoard && cannotBoard(other->type()))) {
            continue;
        }
        if (!Aabb::of(other->x, other->y, other->z, other->width(), other->height()).intersects(reach)) {
            continue;
        }
        other->vehicleId = boat.entityId();
        boat.passengerIds.push_back(other->entityId());
        result.boarded.push_back(other);
    }
    return result;
}

} // namespace basalt::server::runtime::entity::boats
