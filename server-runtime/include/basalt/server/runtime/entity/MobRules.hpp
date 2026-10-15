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

/// parity: `MobCategory` off each `EntityType`.
namespace mobs {

inline constexpr double kDefaultWalkSpeed = 0.25;

[[nodiscard]] bool isMob(const wire::common::Identifier& type);

[[nodiscard]] std::string_view categoryOf(const wire::common::Identifier& type);

} // namespace mobs

namespace cubeMobs {

inline constexpr std::int32_t kDefaultSize = 1;
inline constexpr std::int32_t kMinSize = 1;
inline constexpr std::int32_t kMaxSize = 127;

[[nodiscard]] bool isCube(std::string_view typePath);

[[nodiscard]] bool isCube(const wire::common::Identifier& type);

void applySize(ServerEntity& entity);

void applySize(ServerEntity& entity, std::int32_t size);

struct Split {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    std::int32_t size = 1;
};

[[nodiscard]] std::vector<Split> splitsOf(const ServerEntity& cube, random::RandomSource& random);

} // namespace cubeMobs

namespace ageableMobs {

/// parity: `AgeableMob.DATA_BABY_ID`.
inline constexpr std::int32_t kBabyDataIndex = 16;

inline constexpr std::int32_t kBabyStartAge = -24000;

[[nodiscard]] bool isAgeable(std::string_view typePath);

[[nodiscard]] bool isAgeable(const wire::common::Identifier& type);

[[nodiscard]] bool hasBabyMetadata(std::string_view typePath);

[[nodiscard]] bool usesBooleanBabyFlag(std::string_view typePath);

} // namespace ageableMobs

namespace taming {

inline constexpr std::int32_t kTameOdds = 3;

inline constexpr std::int32_t kFlagsIndex = 18;
inline constexpr std::int32_t kFlagSitting = 0x01;
inline constexpr std::int32_t kFlagTamed = 0x04;

inline constexpr std::int32_t kEventTamingFailed = 6;
inline constexpr std::int32_t kEventTamingSucceeded = 7;

[[nodiscard]] bool isTamable(const wire::common::Identifier& type);

[[nodiscard]] bool tamedBy(const wire::common::Identifier& type, const wire::common::Identifier& item);

[[nodiscard]] std::int32_t flagsOf(const ServerEntity& entity);

} // namespace taming

namespace mobPersistence {

[[nodiscard]] bool keeps(const ServerEntity& entity);

[[nodiscard]] bool requiresCustomPersistence(const ServerEntity& entity);

} // namespace mobPersistence

namespace sunBurn {

inline constexpr std::int32_t kFireTicks = 160;

inline constexpr std::int32_t kDamageInterval = 20;
inline constexpr float kFireDamage = 1.0F;

[[nodiscard]] bool burns(const wire::common::Identifier& type);

[[nodiscard]] bool shouldBurn(const wire::common::Identifier& type, bool day, std::int32_t light, bool openSky,
                              bool inWater, random::RandomSource& random);

} // namespace sunBurn

namespace mobDespawn {

/// parity: the `noActionTime > 600` before the random despawn is even considered.
inline constexpr std::int32_t kIdleBeforeRandom = 600;

inline constexpr std::int32_t kRandomOdds = 800;

[[nodiscard]] bool shouldDespawn(ServerEntity& mob, MobCategory category, std::optional<double> nearestDistance,
                                 random::RandomSource& random);

} // namespace mobDespawn

namespace wardens {

inline constexpr std::int32_t kAngry = 80;
inline constexpr std::int32_t kMaxAnger = 150;

inline constexpr std::int32_t kHeard = 35;

inline constexpr std::int32_t kHurt = kAngry + 20;

inline constexpr double kHearingRange = 16.0;

[[nodiscard]] wire::common::Identifier type();

[[nodiscard]] bool isWarden(const wire::common::Identifier& type);

std::int32_t anger(ServerEntity& warden, std::int32_t suspectId, std::int32_t amount);

void tick(ServerEntity& warden);

[[nodiscard]] std::optional<std::int32_t> target(const ServerEntity& warden);

[[nodiscard]] std::int32_t angerAt(const ServerEntity& warden, std::int32_t suspectId);

} // namespace wardens

namespace entityPickResults {

[[nodiscard]] std::optional<wire::common::Identifier> itemFor(const wire::common::Identifier& type);

} // namespace entityPickResults

} // namespace basalt::server::runtime::entity
