#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// parity: `FireworkRocketEntity`'s fields and constructors.
namespace fireworkRockets {

struct RocketState {
    std::int32_t life = 0;
    std::int32_t lifetime = 0;
    /// parity: `DATA_ATTACHED_TO_TARGET`, the entity id of whoever is flying on it.
    std::optional<std::int32_t> attachedTo;
    /// parity: `DATA_SHOT_AT_ANGLE`, from a crossbow or a dispenser: no boost, no climb.
    bool shotAtAngle = false;
    /// parity: `Projectile.leftOwner`.
    bool leftOwner = false;
    wire::nbt::NbtCompound item;
};

[[nodiscard]] const wire::common::Identifier& type();

[[nodiscard]] bool isRocket(const ServerEntity& entity);

/// parity: `new ItemStack(Items.FIREWORK_ROCKET)`, a rocket with nothing on it.
[[nodiscard]] wire::nbt::NbtCompound plainRocket();

/// parity: `CelebrateVillagersSurvivedRaid.getFirework`.
[[nodiscard]] wire::nbt::NbtCompound celebrationRocket(std::int32_t dye, std::int32_t flight);

/// parity: `Fireworks.flightDuration`.
[[nodiscard]] std::int32_t flightDuration(const wire::nbt::NbtCompound& item);

/// parity: `getExplosions().size()`.
[[nodiscard]] std::int32_t explosionCount(const wire::nbt::NbtCompound& item);

RocketState& start(ServerEntity& rocket, wire::nbt::NbtCompound item, random::RandomSource& random);

/// parity: `addAdditionalSaveData`.
void write(const ServerEntity& rocket, wire::nbt::NbtCompound& tag);

/// parity: `readAdditionalSaveData`.
void read(const wire::nbt::NbtCompound& tag, ServerEntity& rocket);

} // namespace fireworkRockets

} // namespace basalt::server::runtime::entity
