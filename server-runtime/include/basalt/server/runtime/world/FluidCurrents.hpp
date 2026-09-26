#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::entity {
class ServerEntity;
} // namespace basalt::server::runtime::entity

namespace basalt::server::runtime::player {
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {

class LoadedWorld;

/// How water and lava carry whatever is in them.
///
/// parity: `EntityFluidInteraction.update` and its `CurrentAccumulator`, over `FlowingFluid.getFlow`.
namespace fluidCurrents {

using BlockAt = std::function<std::optional<std::int32_t>(std::int32_t x, std::int32_t y, std::int32_t z)>;

/// parity: `Entity.updateFluidInteraction`'s scales: water, lava, and lava where it runs fast.
inline constexpr double kWaterScale = 0.014;
inline constexpr double kLavaScale = 0.0023333333333333335;
inline constexpr double kFastLavaScale = 0.007;

/// The flows of one fluid summed over every square that reached the box.
struct Current {
    std::array<double, 3> sum{};
    std::int32_t count = 0;
    double height = 0.0;
};

/// What a box found itself in this turn.
struct Contact {
    /// parity: `getFluidHeight`, how far above the box's bottom each fluid reaches.
    double waterHeight = 0.0;
    double lavaHeight = 0.0;
    Current water;
    Current lava;

    [[nodiscard]] bool inWater() const { return waterHeight > 0.0; }

    [[nodiscard]] bool inLava() const { return lavaHeight > 0.0; }
};

/// parity: `FlowingFluid.getFlow`: which way the fluid in this square runs, as a unit vector.
[[nodiscard]] std::array<double, 3> flowAt(const BlockAt& blockAt, std::int32_t x, std::int32_t y, std::int32_t z,
                                           std::int32_t stateId, const realBlockTags::TagMembers* blockTags);

/// parity: `EntityFluidInteraction.update` for an entity whose bounding box is `box`. The currents
/// are only summed for one `isPushedByFluid`.
[[nodiscard]] Contact touch(const BlockAt& blockAt, const entity::Aabb& box, bool pushed,
                            const realBlockTags::TagMembers* blockTags);

/// parity: `CurrentAccumulator.applyTo`: a player takes the average, anything else the direction.
void push(std::array<double, 3>& movement, const Current& current, bool player, double scale);

/// Both fluids, as `updateFluidInteraction` applies them to one `isPushedByFluid`.
void pushAll(std::array<double, 3>& movement, const Contact& contact, bool player, bool fastLava);

/// parity: `isPushedByFluid` for anything but a player: every water creature, the turtle, the frog,
/// the axolotl and the nautili stay put, and a drowned while it swims.
[[nodiscard]] bool pushedByFluid(const entity::ServerEntity& entity, const Contact& contact);

/// parity: `Entity.updateFluidInteraction` on an entity of the world: its box, or the part of it
/// above the boat it rides, carried by both fluids. Answers what the box was in.
Contact carry(LoadedWorld& world, entity::ServerEntity& entity, const realBlockTags::TagMembers* blockTags);

/// The same for a player, on the motion the server keeps for them. A flying player is not carried.
Contact carry(LoadedWorld& world, player::PlayerSession& player, const realBlockTags::TagMembers* blockTags);

} // namespace fluidCurrents

} // namespace basalt::server::runtime::world
