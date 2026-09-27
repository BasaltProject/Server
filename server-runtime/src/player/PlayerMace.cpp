#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerAttributes.hpp>
#include <basalt/server/runtime/player/PlayerImpulse.hpp>
#include <basalt/server/runtime/player/PlayerMace.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/scoreboard/Teams.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cmath>
#include <vector>

namespace basalt::server::runtime::player::mace {
namespace {

using entity::ServerEntity;
using wire::common::Identifier;
using wire::protocol::play::Gamemode;
using wire::protocol::play::SoundSource;

/// parity: `MaceItem`'s thresholds and its shock wave.
constexpr double kHeavyThreshold = 5.0;
constexpr double kKnockbackRadius = 3.5;
constexpr double kKnockbackPower = 0.7;
constexpr double kKnockbackLift = 0.7;
/// parity: `LevelEvent.PARTICLES_SMASH_ATTACK`, with the particle count it is given.
constexpr std::int32_t kSmashEvent = 2013;
constexpr std::int32_t kSmashParticles = 750;
/// parity: `hurtEnemy` leaves the attacker hanging at a hundredth of a block a tick upwards.
constexpr double kHang = static_cast<double>(0.01F);

[[nodiscard]] std::vector<PlayerSession*> playersIn(const pvp::Context& context, std::string_view worldName) {
    std::vector<PlayerSession*> found;
    if (context.registry == nullptr) {
        return found;
    }
    for (PlayerSession* player : context.registry->snapshot()) {
        if (player->worldName == worldName) {
            found.push_back(player);
        }
    }
    return found;
}

[[nodiscard]] bool allied(const pvp::Context& context, const PlayerSession& attacker, std::string_view member) {
    const scoreboard::Team* own = context.teams == nullptr ? nullptr : context.teams->teamOf(attacker.name());
    return own != nullptr && own == context.teams->teamOf(member);
}

/// parity: `getKnockbackPower`, the power left at that distance, doubled after a heavy fall.
[[nodiscard]] double powerAt(const PlayerSession& attacker, double distance, double resistance) {
    return (kKnockbackRadius - distance) * kKnockbackPower * (attacker.fallDistance > kHeavyThreshold ? 2.0 : 1.0) *
           (1.0 - resistance);
}

/// parity: `MaceItem.knockback`'s predicate and push, over everything alive near the victim.
void shockWave(const pvp::Context& context, world::LoadedWorld& world, PlayerSession& attacker, const Struck& struck) {
    const std::vector<PlayerSession*> players = playersIn(context, attacker.worldName);
    for (PlayerSession* player : players) {
        if (player->connection().sendLevelEvent) {
            player->connection().sendLevelEvent(kSmashEvent, static_cast<std::int32_t>(std::floor(struck.x)),
                                                static_cast<std::int32_t>(std::floor(struck.y - 1.0E-5)),
                                                static_cast<std::int32_t>(std::floor(struck.z)), kSmashParticles);
        }
    }
    const auto push = [&](double x, double y, double z, double resistance) -> std::optional<std::array<double, 3>> {
        const double dx = x - struck.x;
        const double dy = y - struck.y;
        const double dz = z - struck.z;
        const double distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
        if (distance > kKnockbackRadius) {
            return std::nullopt;
        }
        const double power = powerAt(attacker, distance, resistance);
        if (power <= 0.0 || distance < 1.0E-4) {
            return power > 0.0 ? std::optional{std::array<double, 3>{0.0, kKnockbackLift, 0.0}} : std::nullopt;
        }
        return std::array<double, 3>{dx / distance * power, kKnockbackLift, dz / distance * power};
    };
    for (ServerEntity* nearby : world.state().entities()) {
        if (nearby->entityId() == struck.entityId || nearby->removed || nearby->dying ||
            !server::registry::entityAttributes::isLiving(nearby->type()) ||
            allied(context, attacker, nearby->uuid().toString()) ||
            (nearby->type().path() == "armor_stand" && nearby->sourceNbt.has_value() &&
             nearby->sourceNbt->getBoolean("Marker"))) {
            continue;
        }
        // parity: the tame check asks whether the pet belongs to the victim, not to the attacker.
        const std::string_view path = nearby->type().path();
        if ((path == "wolf" || path == "cat" || path == "parrot") && nearby->owner == std::optional{struck.uuid}) {
            continue;
        }
        if (const auto thrown =
                push(nearby->x, nearby->y, nearby->z, nearby->attributeWithModifiers("minecraft:knockback_resistance"));
            thrown.has_value()) {
            nearby->velocityX += (*thrown)[0];
            nearby->velocityY += (*thrown)[1];
            nearby->velocityZ += (*thrown)[2];
        }
    }
    const world::weaponDamage::DefinitionOf none;
    for (PlayerSession* nearby : players) {
        if (nearby == &attacker || nearby->entityId() == struck.entityId || nearby->dead ||
            nearby->gamemode == Gamemode::Spectator || (nearby->gamemode == Gamemode::Creative && nearby->flying) ||
            allied(context, attacker, nearby->name())) {
            continue;
        }
        if (const auto thrown = push(nearby->x, nearby->y, nearby->z,
                                     playerAttributes::valueOf(*nearby, "minecraft:knockback_resistance", none));
            thrown.has_value()) {
            for (std::size_t axis = 0; axis < 3; ++axis) {
                nearby->deltaMovement[axis] += (*thrown)[axis];
            }
            pvp::sendMotion(*nearby, nearby->deltaMovement, 0.0F);
        }
    }
}

} // namespace

bool isMace(const std::optional<storage::StoredItemStack>& weapon) {
    return weapon.has_value() && weapon->id == "minecraft:mace";
}

bool canSmash(const PlayerSession& attacker) { return attacker.fallDistance > kSmashThreshold && !attacker.fallFlying; }

bool smashes(const PlayerSession& attacker, const std::optional<storage::StoredItemStack>& weapon) {
    return isMace(weapon) && canSmash(attacker);
}

float damageBonus(const PlayerSession& attacker, const std::optional<storage::StoredItemStack>& weapon,
                  const world::weaponDamage::DefinitionOf& definitionOf) {
    if (!smashes(attacker, weapon)) {
        return 0.0F;
    }
    const double fall = attacker.fallDistance;
    double damage = 0.0;
    if (fall <= 3.0) {
        damage = 4.0 * fall;
    } else if (fall <= 8.0) {
        damage = 12.0 + (2.0 * (fall - 3.0));
    } else {
        damage = 22.0 + fall - 8.0;
    }
    const float perBlock =
        world::weaponDamage::valueEffect(*weapon, "minecraft:smash_damage_per_fallen_block", 0.0F, definitionOf);
    return static_cast<float>(damage + (static_cast<double>(perBlock) * fall));
}

void hurtEnemy(const pvp::Context& context, world::LoadedWorld& world, PlayerSession& attacker, const Struck& struck) {
    attacker.deltaMovement[1] = kHang;
    // parity: `calculateImpactPosition`, the impulse's own point while it is still below them.
    const std::array<double, 3> here{attacker.x, attacker.y, attacker.z};
    const std::array<double, 3> impact =
        attacker.impulseImpact.has_value() && (*attacker.impulseImpact)[1] <= attacker.y ? *attacker.impulseImpact
                                                                                         : here;
    attacker.impulseGraceTicks = std::max(attacker.impulseGraceTicks, impulse::kGraceTicks);
    attacker.impulseImpact = impact;
    pvp::sendMotion(attacker, attacker.deltaMovement, attacker.yaw);
    const std::string_view sound = !struck.onGround                          ? "item.mace.smash_air"
                                   : attacker.fallDistance > kHeavyThreshold ? "item.mace.smash_ground_heavy"
                                                                             : "item.mace.smash_ground";
    random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    const std::int64_t seed = random.nextLong();
    for (const PlayerSession* player : playersIn(context, attacker.worldName)) {
        playSoundFor(*player, sound, SoundSource::Players, attacker.x, attacker.y, attacker.z, 1.0F, 1.0F, seed);
    }
    shockWave(context, world, attacker, struck);
}

void postHurtEnemy(PlayerSession& attacker) { attacker.fallDistance = 0.0; }

float armorFractionAdd(const std::optional<storage::StoredItemStack>& weapon,
                       const world::weaponDamage::DefinitionOf& definitionOf) {
    return weapon.has_value()
               ? world::weaponDamage::valueEffect(*weapon, "minecraft:armor_effectiveness", 0.0F, definitionOf)
               : 0.0F;
}

} // namespace basalt::server::runtime::player::mace
