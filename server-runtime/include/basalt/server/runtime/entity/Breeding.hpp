#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// Feeding two animals and what comes of it.
///
/// parity: `Animal`: the right food puts one in love for six hundred ticks, two in love within eight
/// blocks walk together, and after sixty ticks within three blocks they breed. The parents then wait
/// five minutes before they can again, and the calf is a baby for twenty minutes.
namespace breeding {

/// parity: `Animal.setInLove`, thirty seconds of it.
inline constexpr std::int32_t kLoveTicks = 600;

/// parity: `PARENT_AGE_AFTER_BREEDING`, five minutes before either can breed again.
inline constexpr std::int32_t kParentCooldown = 6000;

/// parity: `AgeableMob.BABY_START_AGE`, twenty minutes of being small.
inline constexpr std::int32_t kBabyAge = -24000;

/// parity: `BreedGoal`: partners are looked for within eight blocks.
inline constexpr double kPartnerRange = 8.0;

/// parity: `BreedGoal.tick`, which needs sixty ticks together and three blocks between them.
inline constexpr std::int32_t kCourtshipTicks = 60;
inline constexpr double kBreedDistanceSquared = 9.0;

/// What this animal eats, for the goal that follows whoever is holding it.
[[nodiscard]] const std::set<std::string, std::less<>>& foodFor(const wire::common::Identifier& type);

/// Whether this animal eats this, which is what puts it in love.
[[nodiscard]] bool isFood(const wire::common::Identifier& type, const wire::common::Identifier& item);

/// parity: `Animal.mobInteract`'s own gate: `age == 0 && canFallInLove()`. A calf is too young and a
/// parent that just bred is counting down, and neither is zero.
[[nodiscard]] bool canFallInLove(const ServerEntity& animal);

/// parity: `setInLove`, which is what a fed animal comes away with. `cause` is the player whose
/// advancement and stat the calf will be, kept as `loveCause` is.
void feed(ServerEntity& animal, std::optional<std::string> cause = std::nullopt);

/// parity: `AgeableMob.canAgeUp`: a baby that is not age locked, and nothing in this build locks one.
[[nodiscard]] bool canAgeUp(const ServerEntity& animal);

/// parity: `getSpeedUpSecondsWhenFeeding` plus `ageUp`: feeding a calf takes a tenth of the wait it
/// has left, counted in whole seconds, and never carries it past grown.
void feedBaby(ServerEntity& animal);

/// parity: `canMate`: both in love, the same kind, and not the same animal twice.
[[nodiscard]] bool canMate(const ServerEntity& animal, const ServerEntity& other);

/// One tick of being in love. Answers the partner it should breed with now, or nothing.
///
/// parity: `Animal.aiStep`'s countdown and `BreedGoal.tick`'s sixty ticks: they have to be near each
/// other for three seconds before anything comes of it.
[[nodiscard]] ServerEntity* tick(ServerEntity& animal, const std::vector<ServerEntity*>& nearby);

/// parity: `spawnChildFromBreeding`: both parents come out of love and on cooldown.
void afterBreeding(ServerEntity& animal, ServerEntity& partner);

/// parity: `AgeableMob.ageUp`, one tick at a time until it is grown.
void growUp(ServerEntity& animal);

} // namespace breeding

} // namespace basalt::server::runtime::entity
