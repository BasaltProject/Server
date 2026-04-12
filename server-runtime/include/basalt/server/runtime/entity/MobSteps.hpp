#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// What a mob's own movement makes heard and felt: footsteps, swimming and the fall that ends on
/// the ground.
///
/// parity: `Entity.getMovementEmission`, `playStepSound`, `getSwimSound`, `playSwimSound`,
/// `checkFallDamage`, and the overrides of each.
namespace mobSteps {

/// parity: `Entity.MovementEmission`.
enum class Emission : std::uint8_t { None, Events, All };

[[nodiscard]] Emission emissionOf(const wire::common::Identifier& type);

/// A sound, its volume and its pitch. An empty name is silence.
struct Sound {
    std::string name;
    float volume = 1.0F;
    float pitch = 1.0F;
};

/// Whether a block is in a block tag.
using Tagged = std::function<bool(std::string_view tag, std::int32_t stateId)>;

/// parity: the mob's `playStepSound` on `stateId`, with `above` the block over it. Nothing when the
/// block's own step sound is the mob's.
[[nodiscard]] std::optional<Sound> ownStep(const ServerEntity& mob, std::int32_t stateId, std::int32_t above,
                                           bool inLava, const Tagged& tagged);

/// parity: `Entity.playStepSound`, the block's own step at fifteen hundredths of its volume.
[[nodiscard]] Sound blockStep(std::int32_t stateId);

/// parity: `getSwimSound`.
[[nodiscard]] std::string swimSound(const ServerEntity& mob);

/// parity: `playSwimSound`'s volume, which the turtle and the skeleton horse change.
[[nodiscard]] float swimVolume(const ServerEntity& mob, float volume);

/// parity: an empty `checkFallDamage`, which the fliers have: they never count a fall.
[[nodiscard]] bool countsFalls(const wire::common::Identifier& type);

/// parity: `getFallSounds`, small or big past four points of damage.
[[nodiscard]] std::string fallSound(const wire::common::Identifier& type, std::int32_t damage);

/// parity: `calculateFallDamage`'s own reduction, ten for a goat and five for a frog.
[[nodiscard]] std::int32_t fallDamageReduction(const wire::common::Identifier& type);

} // namespace mobSteps

} // namespace basalt::server::runtime::entity
