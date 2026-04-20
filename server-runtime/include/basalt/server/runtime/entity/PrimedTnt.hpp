#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// parity: `PrimedTnt`.
namespace primedTnt {

[[nodiscard]] const wire::common::Identifier& type();

inline constexpr std::int32_t kDefaultFuse = 80;

inline constexpr float kExplosionPower = 4.0F;

inline constexpr double kExplosionOffset = 0.0625;

inline constexpr double kPrimeHop = 0.2;
inline constexpr double kPrimeDrift = 0.02;

ServerEntity& prime(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t entityId,
                    std::int32_t protocolType, double angle, std::int32_t fuse = kDefaultFuse);

/// parity: `PrimedTnt.tick`.
bool tick(world::WorldState& world, ServerEntity& entity);

} // namespace primedTnt

} // namespace basalt::server::runtime::entity
