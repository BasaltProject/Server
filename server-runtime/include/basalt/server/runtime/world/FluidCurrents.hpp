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

/// parity: `EntityFluidInteraction.update` and its `CurrentAccumulator`.
namespace fluidCurrents {

using BlockAt = std::function<std::optional<std::int32_t>(std::int32_t x, std::int32_t y, std::int32_t z)>;

/// parity: `Entity.updateFluidInteraction`'s scales.
inline constexpr double kWaterScale = 0.014;
inline constexpr double kLavaScale = 0.0023333333333333335;
inline constexpr double kFastLavaScale = 0.007;

struct Current {
    std::array<double, 3> sum{};
    std::int32_t count = 0;
    double height = 0.0;
};

struct Contact {
    /// parity: `getFluidHeight`, how far above the box's bottom each fluid reaches.
    double waterHeight = 0.0;
    double lavaHeight = 0.0;
    Current water;
    Current lava;

    [[nodiscard]] bool inWater() const { return waterHeight > 0.0; }

    [[nodiscard]] bool inLava() const { return lavaHeight > 0.0; }
};

/// parity: `FlowingFluid.getFlow`.
[[nodiscard]] std::array<double, 3> flowAt(const BlockAt& blockAt, std::int32_t x, std::int32_t y, std::int32_t z,
                                           std::int32_t stateId, const realBlockTags::TagMembers* blockTags);

/// parity: `EntityFluidInteraction.update`.
[[nodiscard]] Contact touch(const BlockAt& blockAt, const entity::Aabb& box, bool pushed,
                            const realBlockTags::TagMembers* blockTags);

/// parity: `CurrentAccumulator.applyTo`.
void push(std::array<double, 3>& movement, const Current& current, bool player, double scale);

void pushAll(std::array<double, 3>& movement, const Contact& contact, bool player, bool fastLava);

/// parity: `isPushedByFluid`.
[[nodiscard]] bool pushedByFluid(const entity::ServerEntity& entity, const Contact& contact);

/// parity: `Entity.updateFluidInteraction`: the box, or the part above the boat it rides.
Contact carry(LoadedWorld& world, entity::ServerEntity& entity, const realBlockTags::TagMembers* blockTags);

/// A flying player is not carried.
Contact carry(LoadedWorld& world, player::PlayerSession& player, const realBlockTags::TagMembers* blockTags);

} // namespace fluidCurrents

} // namespace basalt::server::runtime::world
