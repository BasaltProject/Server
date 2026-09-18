#include <basalt/server/runtime/entity/MobConversions.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <array>

namespace basalt::server::runtime::entity::mobConversions {
namespace {

using namespace std::string_view_literals;
using wire::common::Identifier;
using wire::nbt::NbtCompound;

/// parity: `Zombie.tick`'s thirty seconds under water, then fifteen of shaking.
constexpr std::int32_t kInWaterTicks = 600;
constexpr std::int32_t kDrownedCountdown = 300;
/// parity: `Skeleton.tick`'s seven seconds in powder snow, then fifteen of shaking.
constexpr std::int32_t kInPowderSnowTicks = 140;
constexpr std::int32_t kStrayCountdown = 300;
/// parity: the fifteen seconds a piglin or a hoglin lasts outside the Nether.
constexpr std::int32_t kZombifyTicks = 300;

/// parity: `LevelEvent.SOUND_ZOMBIE_CONVERTED_TO_DROWNED` and its two neighbours.
constexpr std::int32_t kDrownedEvent = 1040;
constexpr std::int32_t kHuskEvent = 1041;
constexpr std::int32_t kStrayEvent = 1048;
constexpr std::int32_t kCuredEvent = 1027;

/// parity: what `convertCommon` copies whatever the two mobs are.
constexpr std::array kCommonKeys{"AbsorptionAmount"sv,
                                 "active_effects"sv,
                                 "LeftHanded"sv,
                                 "NoAI"sv,
                                 "PersistenceRequired"sv,
                                 "CustomNameVisible"sv,
                                 "Invulnerable"sv,
                                 "NoGravity"sv,
                                 "PortalCooldown"sv,
                                 "Silent"sv,
                                 "Tags"sv,
                                 "CustomName"sv,
                                 "data"sv};
/// parity: what `SINGLE` adds: where it is and how it is moving.
constexpr std::array kPlaceKeys{"Pos"sv,      "Motion"sv,   "Rotation"sv,  "fall_distance"sv,
                                "OnGround"sv, "HurtTime"sv, "FallFlying"sv};

[[nodiscard]] bool convertsInWater(std::string_view path) { return path == "zombie" || path == "husk"; }

[[nodiscard]] bool zombifies(std::string_view path) {
    return path == "piglin" || path == "piglin_brute" || path == "hoglin";
}

} // namespace

std::optional<std::uint8_t> shakingIndexOf(std::string_view typePath) {
    // parity: `Zombie.DATA_DROWNED_CONVERSION_ID` after its baby and special type flags, and
    // `Skeleton.DATA_STRAY_CONVERSION_ID` right after `Mob`'s own.
    if (typePath == "zombie" || typePath == "husk") {
        return 18;
    }
    // parity: `ZombieVillager.DATA_CONVERTING_ID`, right after `Zombie`'s three.
    if (typePath == "zombie_villager") {
        return 19;
    }
    return typePath == "skeleton" ? std::optional<std::uint8_t>{16} : std::nullopt;
}

bool isShaking(const ServerEntity& mob) {
    return shakingIndexOf(mob.type().path()).has_value() && mob.conversionCountdown >= 0;
}

std::optional<Conversion> tick(ServerEntity& mob, const Around& around) {
    const std::string_view path = mob.type().path();
    const bool alive = !mob.dying && mob.health > 0.0 && !mob.removed;
    if (convertsInWater(path)) {
        if (!alive || mob.noAi) {
            return std::nullopt;
        }
        if (mob.conversionCountdown >= 0) {
            if (--mob.conversionCountdown < 0) {
                return path == "husk" ? Conversion{.into = Identifier::vanilla("zombie"), .levelEvent = kHuskEvent}
                                      : Conversion{.into = Identifier::vanilla("drowned"), .levelEvent = kDrownedEvent};
            }
        } else if (around.eyeInWater) {
            if (++mob.conversionClock >= kInWaterTicks) {
                mob.conversionCountdown = kDrownedCountdown;
            }
        } else {
            mob.conversionClock = -1;
        }
        return std::nullopt;
    }
    // parity: `ZombieVillager.tick`, which runs whether it has AI or not.
    if (path == "zombie_villager") {
        if (alive && mob.conversionCountdown >= 0) {
            mob.conversionCountdown -= around.cureProgress;
            if (mob.conversionCountdown <= 0) {
                return Conversion{.into = Identifier::vanilla("villager"),
                                  .keepEquipment = false,
                                  .keepCanPickUpLoot = false,
                                  .levelEvent = kCuredEvent,
                                  .nausea = true};
            }
        }
        return std::nullopt;
    }
    if (path == "skeleton") {
        if (!alive || mob.noAi) {
            return std::nullopt;
        }
        if (!around.inPowderSnow) {
            mob.conversionClock = -1;
            mob.conversionCountdown = -1;
        } else if (mob.conversionCountdown >= 0) {
            if (--mob.conversionCountdown < 0) {
                return Conversion{.into = Identifier::vanilla("stray"), .levelEvent = kStrayEvent};
            }
        } else if (++mob.conversionClock >= kInPowderSnowTicks) {
            mob.conversionCountdown = kStrayCountdown;
        }
        return std::nullopt;
    }
    if (zombifies(path)) {
        const bool converting = !mob.immuneToZombification && !mob.noAi && around.piglinsZombify;
        mob.conversionClock = converting ? mob.conversionClock + 1 : 0;
        if (mob.conversionClock > kZombifyTicks) {
            const bool hoglin = path == "hoglin";
            return Conversion{.into = Identifier::vanilla(hoglin ? "zoglin" : "zombified_piglin"),
                              .keepCanPickUpLoot = !hoglin,
                              .nausea = true};
        }
    }
    return std::nullopt;
}

NbtCompound convertedTag(const NbtCompound& from, const Identifier& fromType, const Conversion& conversion) {
    NbtCompound to;
    to.putString("id", conversion.into.toString());
    const auto copy = [&from, &to](std::string_view key) {
        if (const wire::nbt::NbtTag* value = from.find(key); value != nullptr) {
            to.set(key, *value);
        }
    };
    for (const std::string_view key : kPlaceKeys) {
        copy(key);
    }
    for (const std::string_view key : kCommonKeys) {
        copy(key);
    }
    if (conversion.keepEquipment) {
        copy("equipment");
        copy("drop_chances");
    }
    if (conversion.keepCanPickUpLoot) {
        copy("CanPickUpLoot");
    }
    // parity: a baby stays a baby, and an ageable mob keeps its age when it becomes another one.
    const bool fromAgeable = ageableMobs::isAgeable(fromType.path());
    const bool baby = from.getBoolean("IsBaby") || (fromAgeable && from.getInt("Age") < 0);
    if (fromAgeable && ageableMobs::isAgeable(conversion.into.path())) {
        copy("Age");
        copy("ForcedAge");
    } else if (baby) {
        to.putBoolean("IsBaby", true);
    }
    // parity: `convertCommon` carries a zombie's door breaking into the zombie it becomes.
    if (from.getBoolean("CanBreakDoors")) {
        to.putBoolean("CanBreakDoors", true);
    }
    return to;
}

} // namespace basalt::server::runtime::entity::mobConversions
