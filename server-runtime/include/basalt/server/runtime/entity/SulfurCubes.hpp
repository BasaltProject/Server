#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// parity: `SulfurCube` and the explosive `SulfurCubeArchetype`.
namespace sulfurCubes {

inline constexpr std::int32_t kExplosionPower = 3;
inline constexpr std::int32_t kExplosionFuse = 120;

[[nodiscard]] bool isSulfurCube(const ServerEntity& mob);

[[nodiscard]] bool primed(const ServerEntity& cube);

[[nodiscard]] bool canExplode(const ServerEntity& cube);

/// parity: `SulfurCube.primeTime`, the fuse set and the cube made permanently invulnerable.
bool prime(ServerEntity& cube, bool imminent, random::RandomSource& random);

/// parity: `SulfurCube.hurtServer`, fire lights the fuse and an explosion shortens it.
void primeOnHit(ServerEntity& cube, std::string_view cause);

/// parity: `DamageTypeTags.SULFUR_CUBE_WITH_BLOCK_IMMUNE_TO`, which a cube holding something shrugs off.
[[nodiscard]] bool immuneTo(const ServerEntity& cube, std::string_view cause);

} // namespace sulfurCubes

} // namespace basalt::server::runtime::entity
