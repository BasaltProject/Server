#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::world {

class WorldState;

/// parity: `EndCrystal`.
namespace endCrystals {

inline constexpr std::uint8_t kBeamTargetDataIndex = 8;
inline constexpr std::uint8_t kShowBottomDataIndex = 9;

inline constexpr float kExplosionPower = 6.0F;

[[nodiscard]] bool isCrystal(const wire::common::Identifier& type);

[[nodiscard]] std::optional<std::array<std::int32_t, 3>> beamTarget(const entity::ServerEntity& crystal);

void setBeamTarget(entity::ServerEntity& crystal, std::optional<std::array<std::int32_t, 3>> target);

[[nodiscard]] bool showsBottom(const entity::ServerEntity& crystal);

enum class Hit : std::uint8_t {
    Ignored,
    Removed,
    Explodes,
};

/// parity: `EndCrystal.hurtServer`.
[[nodiscard]] Hit hurt(entity::ServerEntity& crystal, bool byDragon, bool byExplosion, bool byCreative = false);

[[nodiscard]] bool isInvulnerable(const entity::ServerEntity& crystal);

void setInvulnerable(entity::ServerEntity& crystal, bool invulnerable);

/// parity: `EndCrystal.setShowBottom`.
void setShowBottom(entity::ServerEntity& crystal, bool show);

/// parity: `EndCrystal.tick` through `BaseFireBlock.getState`.
std::optional<std::int32_t> keepFireLit(WorldState& world, const entity::ServerEntity& crystal, std::int32_t delay);

} // namespace endCrystals

} // namespace basalt::server::runtime::world
