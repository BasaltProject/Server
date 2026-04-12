#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/FluidCurrents.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace basalt::server::runtime::world::fluidCurrents {
namespace {

using registry::BlockFace;

/// parity: `Direction.Plane.HORIZONTAL`'s order, which is the order the flow is summed in.
struct Step {
    std::int32_t dx;
    std::int32_t dz;
    BlockFace face;
};

constexpr std::array<Step, 4> kHorizontal{
    {{0, -1, BlockFace::North}, {1, 0, BlockFace::East}, {0, 1, BlockFace::South}, {-1, 0, BlockFace::West}}};

/// parity: `Vec3.normalize`, which answers zero under a hundred-thousandth.
[[nodiscard]] std::array<double, 3> normalized(const std::array<double, 3>& vector) {
    const double length = std::sqrt((vector[0] * vector[0]) + (vector[1] * vector[1]) + (vector[2] * vector[2]));
    if (length < static_cast<double>(1.0E-5F)) {
        return {};
    }
    return {vector[0] / length, vector[1] / length, vector[2] / length};
}

/// parity: `FlowingFluid.affectsFlow`: no fluid, or the same one.
[[nodiscard]] bool affectsFlow(const std::optional<FluidKind>& neighbour, FluidKind kind) {
    return !neighbour.has_value() || *neighbour == kind;
}

/// parity: `FlowingFluid.isSolidFace`, whose sturdiness test looks at the face pointing onwards.
[[nodiscard]] bool solidFace(const BlockAt& blockAt, std::int32_t x, std::int32_t y, std::int32_t z, BlockFace face,
                             FluidKind kind) {
    const std::int32_t state = blockAt(x, y, z).value_or(0);
    if (fluidFlow::kindOf(state) == kind) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(state);
    // parity: `IceBlock`, frosted ice included, never counts.
    if (block != nullptr && (block->path() == "ice" || block->path() == "frosted_ice")) {
        return false;
    }
    return registry::blockShapes::isFaceSturdy(state, face);
}

/// parity: `CurrentAccumulator.applyTo`'s floor, below which a still entity is still carried.
constexpr double kStillSpeed = 0.003;
constexpr double kMinimumPush = 0.0045000000000000005;
/// parity: `EntityFluidInteraction.update` scales a flow down by a shallow fluid's height.
constexpr double kShallow = 0.4;
constexpr double kDeflate = 0.001;

} // namespace

std::array<double, 3> flowAt(const BlockAt& blockAt, std::int32_t x, std::int32_t y, std::int32_t z,
                             std::int32_t stateId, const realBlockTags::TagMembers* blockTags) {
    const std::optional<FluidKind> kind = fluidFlow::kindOf(stateId);
    if (!kind.has_value()) {
        return {};
    }
    const float own = fluidFlow::ownHeightOf(stateId);
    double flowX = 0.0;
    double flowZ = 0.0;
    for (const Step& step : kHorizontal) {
        const std::int32_t nx = x + step.dx;
        const std::int32_t nz = z + step.dz;
        const std::int32_t neighbour = blockAt(nx, y, nz).value_or(0);
        const std::optional<FluidKind> neighbourKind = fluidFlow::kindOf(neighbour);
        if (!affectsFlow(neighbourKind, *kind)) {
            continue;
        }
        float neighbourHeight = fluidFlow::ownHeightOf(neighbour);
        float distance = 0.0F;
        if (neighbourHeight == 0.0F) {
            const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(neighbour);
            const bool blocks = block != nullptr && blockTags != nullptr &&
                                realBlockTags::contains(*blockTags, "minecraft:blocks_fluid_flow", *block);
            if (!blocks) {
                const std::int32_t below = blockAt(nx, y - 1, nz).value_or(0);
                if (affectsFlow(fluidFlow::kindOf(below), *kind)) {
                    neighbourHeight = fluidFlow::ownHeightOf(below);
                    if (neighbourHeight > 0.0F) {
                        distance = own - (neighbourHeight - 0.8888889F);
                    }
                }
            }
        } else if (neighbourHeight > 0.0F) {
            distance = own - neighbourHeight;
        }
        if (distance != 0.0F) {
            flowX += static_cast<double>(static_cast<float>(step.dx) * distance);
            flowZ += static_cast<double>(static_cast<float>(step.dz) * distance);
        }
    }
    std::array<double, 3> flow{flowX, 0.0, flowZ};
    if (fluidFlow::fallingFluid(stateId)) {
        for (const Step& step : kHorizontal) {
            if (solidFace(blockAt, x + step.dx, y, z + step.dz, step.face, *kind) ||
                solidFace(blockAt, x + step.dx, y + 1, z + step.dz, step.face, *kind)) {
                flow = normalized(flow);
                flow[1] -= 6.0;
                break;
            }
        }
    }
    return normalized(flow);
}

Contact touch(const BlockAt& blockAt, const entity::Aabb& bounds, bool pushed,
              const realBlockTags::TagMembers* blockTags) {
    Contact contact;
    const entity::Aabb box{.minX = bounds.minX + kDeflate,
                           .minY = bounds.minY + kDeflate,
                           .minZ = bounds.minZ + kDeflate,
                           .maxX = bounds.maxX - kDeflate,
                           .maxY = bounds.maxY - kDeflate,
                           .maxZ = bounds.maxZ - kDeflate};
    const std::int32_t x0 = wire::common::mth::floor(box.minX);
    const std::int32_t y0 = wire::common::mth::floor(box.minY);
    const std::int32_t z0 = wire::common::mth::floor(box.minZ);
    const auto x1 = static_cast<std::int32_t>(std::ceil(box.maxX)) - 1;
    const auto y1 = static_cast<std::int32_t>(std::ceil(box.maxY)) - 1;
    const auto z1 = static_cast<std::int32_t>(std::ceil(box.maxZ)) - 1;
    // parity: one tracker per fluid type, still and flowing apart; one current per fluid tag.
    std::array<double, 4> trackers{};
    std::optional<std::size_t> last;
    Current* current = nullptr;
    for (std::int32_t x = x0; x <= x1; ++x) {
        for (std::int32_t y = y0; y <= y1; ++y) {
            for (std::int32_t z = z0; z <= z1; ++z) {
                const std::int32_t state = blockAt(x, y, z).value_or(0);
                const std::optional<FluidKind> kind = fluidFlow::kindOf(state);
                if (!kind.has_value()) {
                    continue;
                }
                const double top =
                    y + static_cast<double>(fluidFlow::heightOf(state, blockAt(x, y + 1, z).value_or(0)));
                if (top < box.minY) {
                    continue;
                }
                const std::size_t type = (*kind == FluidKind::Water ? 0U : 2U) + (isSourceFluid(state) ? 0U : 1U);
                if (last != type) {
                    last = type;
                    if (pushed) {
                        current = *kind == FluidKind::Water ? &contact.water : &contact.lava;
                    }
                }
                trackers[type] = std::max(top - bounds.minY, trackers[type]);
                if (current != nullptr) {
                    std::array<double, 3> flow = flowAt(blockAt, x, y, z, state, blockTags);
                    current->height = std::max(trackers[type], current->height);
                    if (current->height < kShallow) {
                        for (double& axis : flow) {
                            axis *= current->height;
                        }
                    }
                    for (std::size_t axis = 0; axis < 3; ++axis) {
                        current->sum[axis] += flow[axis];
                    }
                    ++current->count;
                }
            }
        }
    }
    contact.waterHeight = std::max(trackers[0], trackers[1]);
    contact.lavaHeight = std::max(trackers[2], trackers[3]);
    return contact;
}

void push(std::array<double, 3>& movement, const Current& current, bool player, double scale) {
    const double lengthSquared =
        (current.sum[0] * current.sum[0]) + (current.sum[1] * current.sum[1]) + (current.sum[2] * current.sum[2]);
    if (current.count == 0 || lengthSquared < static_cast<double>(1.0E-5F)) {
        return;
    }
    std::array<double, 3> impulse = current.sum;
    if (player) {
        for (double& axis : impulse) {
            axis *= 1.0 / static_cast<double>(current.count);
        }
    } else {
        impulse = normalized(impulse);
    }
    for (double& axis : impulse) {
        axis *= scale;
    }
    const double length = std::sqrt((impulse[0] * impulse[0]) + (impulse[1] * impulse[1]) + (impulse[2] * impulse[2]));
    if (std::abs(movement[0]) < kStillSpeed && std::abs(movement[2]) < kStillSpeed && length < kMinimumPush) {
        impulse = normalized(impulse);
        for (double& axis : impulse) {
            axis *= kMinimumPush;
        }
    }
    for (std::size_t axis = 0; axis < 3; ++axis) {
        movement[axis] += impulse[axis];
    }
}

void pushAll(std::array<double, 3>& movement, const Contact& contact, bool player, bool fastLava) {
    if (contact.inWater()) {
        push(movement, contact.water, player, kWaterScale);
    }
    if (contact.inLava()) {
        push(movement, contact.lava, player, fastLava ? kFastLavaScale : kLavaScale);
    }
}

namespace {

[[nodiscard]] BlockAt blocksOf(LoadedWorld& world) {
    return [&world](std::int32_t x, std::int32_t y, std::int32_t z) { return world.state().blockState(x, y, z); };
}

[[nodiscard]] bool fastLava(const LoadedWorld& world) {
    return world.declaration().dimension == config::WorldDimension::Nether;
}

/// parity: `getFluidInteractionBox` with `AbstractBoat.modifyPassengerFluidInteractionBox`: a rider
/// only meets the fluid above the boat's rim, and none at all while the boat stands taller.
[[nodiscard]] std::optional<entity::Aabb> boxFor(LoadedWorld& world, entity::Aabb box,
                                                 const std::optional<std::int32_t>& vehicleId) {
    const entity::ServerEntity* vehicle = vehicleId.has_value() ? world.state().entity(*vehicleId) : nullptr;
    if (vehicle == nullptr) {
        return box;
    }
    const std::string_view path = vehicle->type().path();
    if (!path.ends_with("_boat") && !path.ends_with("_raft")) {
        return box;
    }
    const double rim = vehicle->y + vehicle->height();
    if (rim >= box.maxY) {
        return std::nullopt;
    }
    box.minY = std::max(box.minY, rim);
    return box;
}

} // namespace

bool pushedByFluid(const entity::ServerEntity& entity, const Contact& contact) {
    static constexpr std::array<std::string_view, 13> kStill{
        "cod",     "salmon", "pufferfish", "tropical_fish", "squid",    "glow_squid",     "dolphin",
        "tadpole", "turtle", "frog",       "axolotl",       "nautilus", "zombie_nautilus"};
    const std::string_view path = entity.type().path();
    if (std::ranges::find(kStill, path) != kStill.end()) {
        return false;
    }
    // parity: `Drowned.isPushedByFluid` is `!isSwimming()`, and it swims under water after a target.
    if (path == "drowned") {
        return !(entity.targetId.has_value() && contact.waterHeight >= entity.height() * 0.85);
    }
    return true;
}

Contact carry(LoadedWorld& world, entity::ServerEntity& entity, const realBlockTags::TagMembers* blockTags) {
    const std::optional<entity::Aabb> box = boxFor(
        world, entity::Aabb::of(entity.x, entity.y, entity.z, entity.width(), entity.height()), entity.vehicleId);
    if (!box.has_value()) {
        return {};
    }
    const BlockAt blocks = blocksOf(world);
    Contact contact = touch(blocks, *box, true, blockTags);
    if (!pushedByFluid(entity, contact)) {
        contact.water = {};
        contact.lava = {};
        return contact;
    }
    std::array<double, 3> movement{entity.velocityX, entity.velocityY, entity.velocityZ};
    pushAll(movement, contact, false, fastLava(world));
    entity.velocityX = movement[0];
    entity.velocityY = movement[1];
    entity.velocityZ = movement[2];
    return contact;
}

Contact carry(LoadedWorld& world, player::PlayerSession& player, const realBlockTags::TagMembers* blockTags) {
    const wire::common::Identifier type = wire::common::Identifier::vanilla("player");
    const double height = player.sneaking ? 1.5 : registry::entityTypeShapes::heightOf(type);
    const std::optional<entity::Aabb> box =
        boxFor(world, entity::Aabb::of(player.x, player.y, player.z, registry::entityTypeShapes::widthOf(type), height),
               player.vehicleId);
    if (!box.has_value()) {
        return {};
    }
    // parity: `Player.isPushedByFluid` is `!abilities.flying`.
    const Contact contact = touch(blocksOf(world), *box, !player.flying, blockTags);
    if (!player.flying) {
        pushAll(player.deltaMovement, contact, true, fastLava(world));
    }
    return contact;
}

} // namespace basalt::server::runtime::world::fluidCurrents
