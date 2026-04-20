#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <span>

namespace basalt::server::runtime::entity {

namespace projectiles {

enum class Flight : std::uint8_t { Arrow, Thrown, Hurting };

struct Kind {
    wire::common::Identifier type;
    double gravity = 0.0;
    double drag = 0.0;
    bool sticks = false;
    double acceleration = 0.0;
    Flight flight = Flight::Thrown;
};

inline constexpr float kDispenserPower = 1.1F;
inline constexpr float kDispenserUncertainty = 6.0F;

inline constexpr double kDispenserReach = 0.7;
inline constexpr double kDispenserLift = 0.1;

struct DispenseShot {
    const Kind* kind = nullptr;
    double reach = kDispenserReach;
    double lift = kDispenserLift;
    float power = kDispenserPower;
    float uncertainty = kDispenserUncertainty;
    bool carries = true;
};

[[nodiscard]] const Kind& arrow();

/// parity: `AbstractHurtingProjectile`.
[[nodiscard]] const Kind& fireball();

[[nodiscard]] const Kind& snowball();

[[nodiscard]] std::span<const Kind> all();

[[nodiscard]] std::optional<DispenseShot> dispensed(const wire::common::Identifier& item);

[[nodiscard]] const Kind* kindOf(const wire::common::Identifier& type);

ServerEntity& spawn(world::WorldState& world, const Kind& kind, std::int32_t entityId, std::int32_t protocolType,
                    double x, double y, double z, double dirX, double dirY, double dirZ, float power, float uncertainty,
                    random::RandomSource& random);

void shoot(ServerEntity& entity, double dirX, double dirY, double dirZ, float power, float uncertainty,
           random::RandomSource& random);

[[nodiscard]] std::optional<BlockHit> tick(world::WorldState& world, ServerEntity& entity, const Kind& kind);

[[nodiscard]] std::optional<double> clipBox(double fromX, double fromY, double fromZ, double toX, double toY,
                                            double toZ, double minX, double minY, double minZ, double maxX, double maxY,
                                            double maxZ);

} // namespace projectiles

} // namespace basalt::server::runtime::entity
