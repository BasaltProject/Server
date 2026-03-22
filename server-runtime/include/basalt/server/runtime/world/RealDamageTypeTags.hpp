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

/// The damage type tags out of the operator's own `server.jar`.
///
/// They are what decides whether a hit goes through armour at all: starving, drowning, withering
/// and almost everything else this build can do to a player is in `bypasses_armor`, and reading
/// that from the jar is the difference between armour meaning something and armour meaning
/// everything.
namespace realDamageTypeTags {

/// The jar's directory listing and its file reader, as callbacks rather than the jar itself: this
/// is the only thing here that needs the operator's own copy of the game, and a test has none.
struct JarReader {
    std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
    std::function<std::optional<std::string>(std::string_view name)> readText;
};

/// Every tag's members, by the tag's own name.
using TagMembers = std::map<wire::common::Identifier, std::set<wire::common::Identifier>>;

inline const wire::common::Identifier kBypassesArmour = wire::common::Identifier::vanilla("bypasses_armor");
inline const wire::common::Identifier kBypassesEffects = wire::common::Identifier::vanilla("bypasses_effects");
inline const wire::common::Identifier kBypassesResistance = wire::common::Identifier::vanilla("bypasses_resistance");
inline const wire::common::Identifier kBypassesEnchantments =
    wire::common::Identifier::vanilla("bypasses_enchantments");

/// Reads and flattens every damage type tag file in the jar. Empty when there is no jar, and empty
/// again when the graph does not resolve, which is a world where armour stops everything.
[[nodiscard]] TagMembers load(const JarReader& jar, const wire::common::Logger& logger = wire::common::Logger{"Tags"});

/// Everything in a tag. Empty for one that is unknown.
[[nodiscard]] const std::set<wire::common::Identifier>& membersOf(const TagMembers& tags,
                                                                  const wire::common::Identifier& tag);

[[nodiscard]] bool isIn(const TagMembers& tags, const wire::common::Identifier& tag,
                        const wire::common::Identifier& damageType);

} // namespace realDamageTypeTags

/// The damage types this build can deal, by the names the jar's own tags use.
namespace damageTypes {

inline const wire::common::Identifier kOutsideBorder = wire::common::Identifier::vanilla("outside_border");
inline const wire::common::Identifier kDrown = wire::common::Identifier::vanilla("drown");
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

/// parity: `DamageTypes.BAD_RESPAWN_POINT`, what a bed in the wrong dimension kills you with.
inline const wire::common::Identifier kBadRespawnPoint = wire::common::Identifier::vanilla("bad_respawn_point");

/// parity: `DamageTypes.WIND_CHARGE`, the single point a gust deals on its way past.
inline const wire::common::Identifier kWindCharge = wire::common::Identifier::vanilla("wind_charge");

/// parity: `DamageTypes.EXPLOSION`, what TNT with nobody to blame kills you with.
inline const wire::common::Identifier kExplosion = wire::common::Identifier::vanilla("explosion");

/// parity: `DamageTypes.PLAYER_ATTACK`, a player's own hand or weapon.
inline const wire::common::Identifier kPlayerAttack = wire::common::Identifier::vanilla("player_attack");

/// parity: `DamageTypes.MOB_ATTACK`, what a mob that walked up to you deals.
inline const wire::common::Identifier kMobAttack = wire::common::Identifier::vanilla("mob_attack");

/// parity: `DamageTypes.ENDER_PEARL`, the five points a pearl costs whoever threw it.
inline const wire::common::Identifier kEnderPearl = wire::common::Identifier::vanilla("ender_pearl");

/// parity: `DamageTypes.ARROW`, which blames whoever shot it.
inline const wire::common::Identifier kArrow = wire::common::Identifier::vanilla("arrow");

} // namespace damageTypes

} // namespace basalt::server::runtime::world
