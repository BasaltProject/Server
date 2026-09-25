#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// One mob turning into another in place: a zombie that drowns, a skeleton that freezes, a piglin
/// that leaves the Nether.
///
/// parity: `Mob.convertTo` with `ConversionType.SINGLE`, and the clocks that call it.
namespace mobConversions {

/// What the clocks read this tick.
struct Around {
    /// parity: `isEyeInFluid(WATER)`.
    bool eyeInWater = false;
    /// parity: `Entity.isInPowderSnow`.
    bool inPowderSnow = false;
    /// parity: `EnvironmentAttributes.PIGLINS_ZOMBIFY`, off only in the Nether.
    bool piglinsZombify = false;
    /// parity: `ZombieVillager.getConversionProgress`, what a cure moves on by this tick.
    std::int32_t cureProgress = 1;
};

/// A conversion that is due now.
struct Conversion {
    wire::common::Identifier into;
    /// parity: `ConversionParams.keepEquipment` and `preserveCanPickUpLoot`.
    bool keepEquipment = true;
    bool keepCanPickUpLoot = true;
    /// The level event it is heard as, zero for none.
    std::int32_t levelEvent = 0;
    /// parity: the nausea a zombified piglin or a zoglin wakes up with.
    bool nausea = false;
    /// parity: `setPersistenceRequired` on what comes out, which a grown tadpole's frog gets.
    bool persistent = false;
    /// The sound the old mob makes as it goes, empty for none, and whether peace keeps it quiet.
    std::string_view sound;
    float soundVolume = 1.0F;
    bool quietInPeace = false;
    /// parity: the `finalizeSpawn(CONVERSION)` the new mob gets, for the three that call it.
    enum class Finalize : std::uint8_t { None, Frog, CuredVillager, ZombieVillager };
    Finalize finalize = Finalize::None;
};

/// The synched flag that makes the client shake it, as an index, or nothing for a mob without one.
[[nodiscard]] std::optional<std::uint8_t> shakingIndexOf(std::string_view typePath);

/// Whether the mob is shaking. parity: `isUnderWaterConverting` and `isFreezeConverting`.
[[nodiscard]] bool isShaking(const ServerEntity& mob);

/// parity: `Tadpole.ticksToBeFrog`, twenty minutes of being one.
inline constexpr std::int32_t kTadpoleTicks = 24000;

/// parity: `ZombieVillager.startConverting`'s three to five minutes.
inline constexpr std::int32_t kCureMinTicks = 3600;
inline constexpr std::int32_t kCureSpread = 2401;

/// Moves the mob's conversion clock on by a tick and answers the conversion when it is due.
[[nodiscard]] std::optional<Conversion> tick(ServerEntity& mob, const Around& around);

/// The saved form of what `from` becomes: position, equipment and the flags a conversion carries,
/// under the new type. It has no uuid, so the one it is read back with is new.
///
/// parity: `ConversionType.SINGLE.convert` and `convertCommon`.
[[nodiscard]] wire::nbt::NbtCompound convertedTag(const wire::nbt::NbtCompound& from,
                                                  const wire::common::Identifier& fromType,
                                                  const Conversion& conversion);

} // namespace mobConversions

} // namespace basalt::server::runtime::entity
