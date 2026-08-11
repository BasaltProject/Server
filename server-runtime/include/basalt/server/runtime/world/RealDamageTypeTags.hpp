#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// They decide whether a hit goes through armour at all (`bypasses_armor` covers starving, drowning, withering).
namespace realDamageTypeTags {

struct JarReader {
    std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
    std::function<std::optional<std::string>(std::string_view name)> readText;
};

using TagMembers = std::map<wire::common::Identifier, std::set<wire::common::Identifier>>;

inline const wire::common::Identifier kBypassesArmour = wire::common::Identifier::vanilla("bypasses_armor");
inline const wire::common::Identifier kBypassesInvulnerability =
    wire::common::Identifier::vanilla("bypasses_invulnerability");
inline const wire::common::Identifier kBypassesEffects = wire::common::Identifier::vanilla("bypasses_effects");
inline const wire::common::Identifier kBypassesResistance = wire::common::Identifier::vanilla("bypasses_resistance");
inline const wire::common::Identifier kBypassesEnchantments =
    wire::common::Identifier::vanilla("bypasses_enchantments");

/// Empty when there is no jar or the graph does not resolve, which is a world where armour stops everything.
[[nodiscard]] TagMembers load(const JarReader& jar, const wire::common::Logger& logger = wire::common::Logger{"Tags"});

[[nodiscard]] const std::set<wire::common::Identifier>& membersOf(const TagMembers& tags,
                                                                  const wire::common::Identifier& tag);

[[nodiscard]] bool isIn(const TagMembers& tags, const wire::common::Identifier& tag,
                        const wire::common::Identifier& damageType);

} // namespace realDamageTypeTags

namespace damageTypes {

inline const wire::common::Identifier kOutsideBorder = wire::common::Identifier::vanilla("outside_border");
inline const wire::common::Identifier kOutOfWorld = wire::common::Identifier::vanilla("out_of_world");
inline const wire::common::Identifier kDrown = wire::common::Identifier::vanilla("drown");
inline const wire::common::Identifier kFreeze = wire::common::Identifier::vanilla("freeze");
inline const wire::common::Identifier kLightningBolt = wire::common::Identifier::vanilla("lightning_bolt");
inline const wire::common::Identifier kSonicBoom = wire::common::Identifier::vanilla("sonic_boom");
inline const wire::common::Identifier kDryOut = wire::common::Identifier::vanilla("dry_out");
inline const wire::common::Identifier kLava = wire::common::Identifier::vanilla("lava");
inline const wire::common::Identifier kOnFire = wire::common::Identifier::vanilla("on_fire");
inline const wire::common::Identifier kMagic = wire::common::Identifier::vanilla("magic");
inline const wire::common::Identifier kWither = wire::common::Identifier::vanilla("wither");
inline const wire::common::Identifier kWitherSkull = wire::common::Identifier::vanilla("wither_skull");
inline const wire::common::Identifier kStarve = wire::common::Identifier::vanilla("starve");
inline const wire::common::Identifier kFall = wire::common::Identifier::vanilla("fall");
/// parity: `DamageTypes.STALAGMITE`, a fall onto the point of pointed dripstone.
inline const wire::common::Identifier kStalagmite = wire::common::Identifier::vanilla("stalagmite");

/// parity: `DamageTypes.BAD_RESPAWN_POINT`.
inline const wire::common::Identifier kBadRespawnPoint = wire::common::Identifier::vanilla("bad_respawn_point");

inline const wire::common::Identifier kWindCharge = wire::common::Identifier::vanilla("wind_charge");

inline const wire::common::Identifier kSpit = wire::common::Identifier::vanilla("spit");

inline const wire::common::Identifier kExplosion = wire::common::Identifier::vanilla("explosion");

inline const wire::common::Identifier kPlayerAttack = wire::common::Identifier::vanilla("player_attack");
inline const wire::common::Identifier kMaceSmash = wire::common::Identifier::vanilla("mace_smash");

inline const wire::common::Identifier kMobAttack = wire::common::Identifier::vanilla("mob_attack");

inline const wire::common::Identifier kSting = wire::common::Identifier::vanilla("sting");

/// parity: `DamageTypes.GENERIC`, what a bee that has lost its sting dies of.
inline const wire::common::Identifier kGeneric = wire::common::Identifier::vanilla("generic");

/// parity: `DamageTypes.ENDER_PEARL`, the five points a pearl costs whoever threw it.
inline const wire::common::Identifier kEnderPearl = wire::common::Identifier::vanilla("ender_pearl");

inline const wire::common::Identifier kFireworks = wire::common::Identifier::vanilla("fireworks");

inline const wire::common::Identifier kArrow = wire::common::Identifier::vanilla("arrow");

inline const wire::common::Identifier kTrident = wire::common::Identifier::vanilla("trident");
inline const wire::common::Identifier kThrown = wire::common::Identifier::vanilla("thrown");

inline const wire::common::Identifier kSpear = wire::common::Identifier::vanilla("spear");

inline const wire::common::Identifier kCramming = wire::common::Identifier::vanilla("cramming");

} // namespace damageTypes

} // namespace basalt::server::runtime::world
