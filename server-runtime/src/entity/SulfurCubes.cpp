#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/MobMisc.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/SulfurCubes.hpp>

#include <algorithm>
#include <array>

namespace basalt::server::runtime::entity::sulfurCubes {
namespace {

[[nodiscard]] bool fireCause(std::string_view cause) {
    return cause == "in_fire" || cause == "on_fire" || cause == "lava" || cause == "campfire" || cause == "hot_floor" ||
           cause == "fireball" || cause == "unattributed_fireball" || cause == "sulfur_cube_hot";
}

[[nodiscard]] bool immuneCause(std::string_view cause) {
    static constexpr std::array<std::string_view, 24> kCauses{
        "arrow",           "cactus",           "dry_out",   "fall",       "falling_anvil",       "falling_block",
        "freeze",          "mace_smash",       "hot_floor", "mob_attack", "mob_attack_no_aggro", "mob_projectile",
        "attack",          "player_attack",    "spear",     "spit",       "stalagmite",          "sting",
        "sulfur_cube_hot", "sweet_berry_bush", "thrown",    "trident",    "wind_charge",         "projectile"};
    return std::ranges::find(kCauses, cause) != kCauses.end() || cause == "explosion" || cause == "player_explosion";
}

} // namespace

bool isSulfurCube(const ServerEntity& mob) { return mob.type().path() == "sulfur_cube"; }

bool primed(const ServerEntity& cube) { return cube.fuse >= 0; }

bool canExplode(const ServerEntity& cube) {
    const mobMisc::State* held = mobMisc::peek(cube);
    return held != nullptr && held->sulfurExplosive && !cube.removed && !cube.dying && cube.health > 0.0 &&
           !primed(cube);
}

bool prime(ServerEntity& cube, bool imminent, random::RandomSource& random) {
    if (!canExplode(cube)) {
        return false;
    }
    // parity: `PrimedTnt.getRandomShortFuse`.
    const std::int32_t fuse =
        imminent ? random.nextInt(std::max(1, kExplosionFuse / 4)) + (kExplosionFuse / 8) : kExplosionFuse;
    if (!cube.sourceNbt.has_value()) {
        cube.sourceNbt = wire::nbt::NbtCompound{};
    }
    cube.sourceNbt->putBoolean("Invulnerable", true);
    ++cube.sourceNbtVersion;
    cube.fuse = fuse;
    return true;
}

bool immuneTo(const ServerEntity& cube, std::string_view cause) {
    const mobMisc::State* held = mobMisc::peek(cube);
    return held != nullptr && held->sulfurBody && immuneCause(cause);
}

void primeOnHit(ServerEntity& cube, std::string_view cause) {
    if (!canExplode(cube)) {
        return;
    }
    const bool fire = fireCause(cause);
    if (!fire && cause != "explosion" && cause != "player_explosion") {
        return;
    }
    random::RandomSource random = random::RandomSource::legacy((static_cast<std::int64_t>(cube.entityId()) * 31) +
                                                               static_cast<std::int64_t>(cube.tickCount));
    static_cast<void>(prime(cube, !fire, random));
}

} // namespace basalt::server::runtime::entity::sulfurCubes
