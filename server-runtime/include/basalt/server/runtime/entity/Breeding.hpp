#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// parity: `Animal`.
namespace breeding {

/// parity: `Animal.setInLove`, thirty seconds of it.
inline constexpr std::int32_t kLoveTicks = 600;

/// parity: `PARENT_AGE_AFTER_BREEDING`, five minutes before either can breed again.
inline constexpr std::int32_t kParentCooldown = 6000;

/// parity: `AgeableMob.BABY_START_AGE`, twenty minutes of being small.
inline constexpr std::int32_t kBabyAge = -24000;

/// parity: `BreedGoal`: partners are looked for within eight blocks.
inline constexpr double kPartnerRange = 8.0;

/// parity: `BreedGoal.tick`.
inline constexpr std::int32_t kCourtshipTicks = 60;
inline constexpr double kBreedDistanceSquared = 9.0;

[[nodiscard]] const std::set<std::string, std::less<>>& foodFor(const wire::common::Identifier& type);

[[nodiscard]] bool isFood(const wire::common::Identifier& type, const wire::common::Identifier& item);

/// parity: `Animal.mobInteract`'s own gate: `age == 0 && canFallInLove()`.
[[nodiscard]] bool canFallInLove(const ServerEntity& animal);

/// parity: `setInLove`, which is what a fed animal comes away with.
void feed(ServerEntity& animal, std::optional<std::string> cause = std::nullopt);

/// parity: `AgeableMob.canAgeUp`.
[[nodiscard]] bool canAgeUp(const ServerEntity& animal);

/// parity: `getSpeedUpSecondsWhenFeeding`.
void feedBaby(ServerEntity& animal);

/// parity: `canMate`: both in love, the same kind, and not the same animal twice.
[[nodiscard]] bool canMate(const ServerEntity& animal, const ServerEntity& other);

/// parity: `Animal.aiStep`.
[[nodiscard]] ServerEntity* tick(ServerEntity& animal, const std::vector<ServerEntity*>& nearby);

/// parity: `spawnChildFromBreeding`: both parents come out of love and on cooldown.
void afterBreeding(ServerEntity& animal, ServerEntity& partner);

/// parity: `AgeableMob.ageUp`, one tick at a time until it is grown.
void growUp(ServerEntity& animal);

} // namespace breeding

} // namespace basalt::server::runtime::entity
