#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// parity: `AreaEffectCloud`.
namespace areaEffectClouds {

/// parity: `DATA_RADIUS`, `DATA_WAITING` and `DATA_PARTICLE`.
inline constexpr std::uint8_t kRadiusDataIndex = 8;
inline constexpr std::uint8_t kWaitingDataIndex = 9;
inline constexpr std::uint8_t kParticleDataIndex = 10;

/// parity: `AreaEffectCloud.DEFAULT_RADIUS` and its 32-block ceiling.
inline constexpr float kDefaultRadius = 3.0F;
inline constexpr float kMaxRadius = 32.0F;
inline constexpr float kMinimalRadius = 0.5F;
inline constexpr float kHeight = 0.5F;
/// parity: `TIME_BETWEEN_APPLICATIONS`.
inline constexpr std::int32_t kTimeBetweenApplications = 5;

[[nodiscard]] bool isCloud(const wire::common::Identifier& type);

struct Effect {
    wire::common::Identifier id = wire::common::Identifier::vanilla("instant_damage");
    std::int32_t amplifier = 0;
    std::int32_t duration = 1;
};

struct Cloud {
    float radius = kDefaultRadius;
    std::int32_t duration = -1;
    std::int32_t waitTime = 20;
    std::int32_t reapplicationDelay = 20;
    std::int32_t durationOnUse = 0;
    float radiusOnUse = 0.0F;
    float radiusPerTick = 0.0F;
    float potionDurationScale = 1.0F;
    std::optional<std::string> particle;
    float particlePower = 1.0F;
    std::optional<Effect> effect;
    std::optional<wire::nbt::NbtTag> owner;
};

[[nodiscard]] Cloud read(const ServerEntity& cloud);

void write(ServerEntity& cloud, const Cloud& settings);

/// parity: the breath `DragonSittingFlamingPhase` and `DragonFireball` leave.
[[nodiscard]] Cloud dragonBreath(float radius, std::int32_t duration, float radiusPerTick, std::int32_t amplifier);

} // namespace areaEffectClouds

} // namespace basalt::server::runtime::entity
