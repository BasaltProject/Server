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

/// The crystals on top of the End's pillars, and the ones a player puts down.
///
/// parity: `EndCrystal`. The beam and the base live in the entity's own tag (`beam_target`,
/// `ShowBottom`), which is where vanilla saves them, so they survive a restart unchanged.
namespace endCrystals {

/// parity: `EndCrystal.DATA_BEAM_TARGET` and `DATA_SHOW_BOTTOM`.
inline constexpr std::uint8_t kBeamTargetDataIndex = 8;
inline constexpr std::uint8_t kShowBottomDataIndex = 9;

/// parity: the blast `EndCrystal.hurtServer` sets off.
inline constexpr float kExplosionPower = 6.0F;

[[nodiscard]] bool isCrystal(const wire::common::Identifier& type);

[[nodiscard]] std::optional<std::array<std::int32_t, 3>> beamTarget(const entity::ServerEntity& crystal);

void setBeamTarget(entity::ServerEntity& crystal, std::optional<std::array<std::int32_t, 3>> target);

[[nodiscard]] bool showsBottom(const entity::ServerEntity& crystal);

/// What a hit does to a crystal.
enum class Hit : std::uint8_t {
    /// Nothing: the dragon's own blows, or a crystal already gone.
    Ignored,
    /// Gone without a blast, because an explosion is what took it.
    Removed,
    /// Gone, and goes off where it stood.
    Explodes,
};

/// parity: `EndCrystal.hurtServer`, whatever the damage: any hit that counts destroys it. An
/// invulnerable crystal (the ones a respawn raises) only answers to a creative player.
[[nodiscard]] Hit hurt(entity::ServerEntity& crystal, bool byDragon, bool byExplosion, bool byCreative = false);

[[nodiscard]] bool isInvulnerable(const entity::ServerEntity& crystal);

void setInvulnerable(entity::ServerEntity& crystal, bool invulnerable);

/// parity: `EndCrystal.setShowBottom`.
void setShowBottom(entity::ServerEntity& crystal, bool show);

/// The fire a crystal keeps under itself while the world has a dragon fight, or nothing when the
/// block was not air. Answers the state written.
///
/// parity: `EndCrystal.tick`, through `BaseFireBlock.getState`.
std::optional<std::int32_t> keepFireLit(WorldState& world, const entity::ServerEntity& crystal, std::int32_t delay);

} // namespace endCrystals

} // namespace basalt::server::runtime::world
