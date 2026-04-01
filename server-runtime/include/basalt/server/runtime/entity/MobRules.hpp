#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/MobCategory.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {

/// Which entity types are alive, and what they are worth to the world around them.
/// parity: `MobCategory` off each `EntityType`; a category says where a type spawns, not whether it is
/// alive (a villager is `misc`). Alive means having `follow_range`, which only a `Mob` has.
namespace mobs {

/// parity: `Attributes.MOVEMENT_SPEED`'s own default for a walking animal.
inline constexpr double kDefaultWalkSpeed = 0.25;

[[nodiscard]] bool isMob(const wire::common::Identifier& type);

/// parity: `MobCategory.getName`, the name a spawn cap is kept under.
[[nodiscard]] std::string_view categoryOf(const wire::common::Identifier& type);

} // namespace mobs

/// The mobs whose size decides what they are.
///
/// parity: `AbstractCubeMob.setSize`: health is the square of the size, speed `0.2 + 0.1 * size` and
/// damage the size itself, never the registry's default speed of 0.7.
namespace cubeMobs {

/// parity: the size `/summon` gives one, which is the smallest.
inline constexpr std::int32_t kDefaultSize = 1;
inline constexpr std::int32_t kMinSize = 1;
inline constexpr std::int32_t kMaxSize = 127;

[[nodiscard]] bool isCube(std::string_view typePath);

[[nodiscard]] bool isCube(const wire::common::Identifier& type);

/// Gives a cube the attributes its size implies. Anything else is left alone.
void applySize(ServerEntity& entity);

void applySize(ServerEntity& entity, std::int32_t size);

/// One of the cubes a dying one leaves behind: where it stands and how big it is.
struct Split {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    std::int32_t size = 1;
};

/// parity: `AbstractCubeMob.remove`, which splits a dead cube bigger than the smallest into two to
/// four halves, each one offset by a quarter of the width it had. Empty for anything else.
[[nodiscard]] std::vector<Split> splitsOf(const ServerEntity& cube, random::RandomSource& random);

} // namespace cubeMobs

/// Entity types that inherit 26.2's `AgeableMob` synchronized baby state.
///
/// Deliberately not the breeding-food table: squid, cubes and villagers are ageable too, while
/// whether Basalt has implemented a particular mating interaction is a separate question.
namespace ageableMobs {

/// parity: `AgeableMob.DATA_BABY_ID`, after the Entity, LivingEntity and PathfinderMob data.
inline constexpr std::int32_t kBabyDataIndex = 16;

/// parity: `AgeableMob.BABY_START_AGE`.
inline constexpr std::int32_t kBabyStartAge = -24000;

[[nodiscard]] bool isAgeable(std::string_view typePath);

[[nodiscard]] bool isAgeable(const wire::common::Identifier& type);

/// Whether the client is told about its age at all, which the zombie family does with a flag
/// instead of a number.
[[nodiscard]] bool hasBabyMetadata(std::string_view typePath);

[[nodiscard]] bool usesBooleanBabyFlag(std::string_view typePath);

} // namespace ageableMobs

/// The mobs a player can make their own.
///
/// parity: `Wolf.mobInteract`, `Cat.mobInteract` and `Ocelot.mobInteract`: the right food in hand,
/// a roll for whether it takes, and from then on the animal has an owner, can be told to sit, and
/// fights whoever fights them.
namespace taming {

/// parity: `Wolf.tame`'s own one in three, which is why a wolf takes a handful of bones.
inline constexpr std::int32_t kTameOdds = 3;

/// parity: `TamableAnimal`'s own byte, whose bits the client reads to draw a sitting pet.
/// `AgeableMob`'s baby and 26.3's `AGE_LOCKED` come first.
inline constexpr std::int32_t kFlagsIndex = 18;
inline constexpr std::int32_t kFlagSitting = 0x01;
inline constexpr std::int32_t kFlagTamed = 0x04;

/// parity: `EntityEvent.TAMING_FAILED` and `TAMING_SUCCEEDED`, the smoke and the hearts.
inline constexpr std::int32_t kEventTamingFailed = 6;
inline constexpr std::int32_t kEventTamingSucceeded = 7;

[[nodiscard]] bool isTamable(const wire::common::Identifier& type);

[[nodiscard]] bool tamedBy(const wire::common::Identifier& type, const wire::common::Identifier& item);

/// The flags byte for this animal, as the client wants it.
[[nodiscard]] std::int32_t flagsOf(const ServerEntity& entity);

} // namespace taming

/// Mobs vanilla excludes from natural caps and despawning without setting `PersistenceRequired`.
///
/// parity: `Mob.requiresCustomPersistence` plus the overrides present in 26.2. Runtime fields are
/// preferred; preserved source NBT covers state Basalt does not yet model directly.
namespace mobPersistence {

/// `isPersistenceRequired || requiresCustomPersistence`.
[[nodiscard]] bool keeps(const ServerEntity& entity);

[[nodiscard]] bool requiresCustomPersistence(const ServerEntity& entity);

} // namespace mobPersistence

/// The undead catching fire in daylight.
///
/// parity: `Mob.burnUndead` over `isSunBurnTick`: the light where it stands has to be over a half,
/// the sky has to be open above it, and even then it is a roll against thirty. Eight seconds of
/// fire a time, which is what makes a zombie caught in the open die rather than walk it off.
namespace sunBurn {

/// parity: `igniteForSeconds(8)`.
inline constexpr std::int32_t kFireTicks = 160;

/// parity: `Entity.baseTick`, which takes a point off every twenty ticks of burning.
inline constexpr std::int32_t kDamageInterval = 20;
inline constexpr float kFireDamage = 1.0F;

[[nodiscard]] bool burns(const wire::common::Identifier& type);

/// Whether this is a tick it catches fire on. `light` is the light where it stands and `openSky`
/// whether anything is over it.
[[nodiscard]] bool shouldBurn(const wire::common::Identifier& type, bool day, std::int32_t light, bool openSky,
                              bool inWater, random::RandomSource& random);

} // namespace sunBurn

/// When a mob is forgotten for being too far from anybody.
///
/// parity: `Mob.checkDespawn`. Past 128 blocks it goes at once; between 32 and 128 on a one in 800
/// roll, only after ten minutes with nobody near. Inside 32 blocks that clock resets.
namespace mobDespawn {

/// parity: the `noActionTime > 600` before the random despawn is even considered.
inline constexpr std::int32_t kIdleBeforeRandom = 600;

/// parity: `random.nextInt(800)`, the roll a far-away mob makes every tick.
inline constexpr std::int32_t kRandomOdds = 800;

/// Whether this mob should be forgotten now. `nearestDistance` is absent when nobody is in the
/// world.
[[nodiscard]] bool shouldDespawn(ServerEntity& mob, MobCategory category, std::optional<double> nearestDistance,
                                 random::RandomSource& random);

} // namespace mobDespawn

/// How angry a warden is at each thing it has noticed.
///
/// parity: `AngerManagement`: anger is per suspect, climbs to 150 at most, falls by one every tick,
/// and only past eighty does the warden come after you. It is the whole reason a warden ignores
/// somebody who has not made a sound, which a target goal reading the follow range does not.
namespace wardens {

/// parity: `AngerLevel.ANGRY`'s own minimum, and the 150 `increaseAnger` clamps to.
inline constexpr std::int32_t kAngry = 80;
inline constexpr std::int32_t kMaxAnger = 150;

/// parity: `increaseAngerAt`'s default, what one vibration is worth.
inline constexpr std::int32_t kHeard = 35;

/// parity: `hurtServer`, which adds the angry threshold plus twenty on top.
inline constexpr std::int32_t kHurt = kAngry + 20;

/// parity: the warden's own `VibrationUser`, which listens sixteen blocks out.
inline constexpr double kHearingRange = 16.0;

[[nodiscard]] wire::common::Identifier type();

[[nodiscard]] bool isWarden(const wire::common::Identifier& type);

/// parity: `increaseAnger`, which tops out at 150 and answers the new level.
std::int32_t anger(ServerEntity& warden, std::int32_t suspectId, std::int32_t amount);

/// parity: `AngerManagement.tick`, one off every suspect every tick and forgotten at one.
void tick(ServerEntity& warden);

/// Whoever it is angriest at, once that anger is enough to come after them.
[[nodiscard]] std::optional<std::int32_t> target(const ServerEntity& warden);

[[nodiscard]] std::int32_t angerAt(const ServerEntity& warden, std::int32_t suspectId);

} // namespace wardens

/// What middle-clicking an entity puts in your hand.
/// parity: `Entity.getPickResult` and overrides: a mob gives its spawn egg, things placed from an
/// item give that item, the rest nothing. An explicit list, since an arrow, a snowball and a lit tnt
/// share an item's name and cannot be picked.
namespace entityPickResults {

[[nodiscard]] std::optional<wire::common::Identifier> itemFor(const wire::common::Identifier& type);

} // namespace entityPickResults

} // namespace basalt::server::runtime::entity
