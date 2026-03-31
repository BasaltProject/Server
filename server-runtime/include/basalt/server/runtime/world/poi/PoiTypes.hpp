#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world::poi {

/// parity: `PoiTypes`, in the order the jar registers them.
enum class PoiType : std::uint8_t {
    kNone,
    kArmorer,
    kButcher,
    kCartographer,
    kCleric,
    kFarmer,
    kFisherman,
    kFletcher,
    kLeatherworker,
    kLibrarian,
    kMason,
    kShepherd,
    kToolsmith,
    kWeaponsmith,
    kHome,
    kMeeting,
    kBeehive,
    kBeeNest,
    kNetherPortal,
    kLodestone,
    kTestInstance,
    kLightningRod,
    kCount,
};

/// A set of types, one bit each, which is what a query takes instead of a predicate.
using PoiMask = std::uint32_t;

[[nodiscard]] constexpr PoiMask maskOf(PoiType type) noexcept { return PoiMask{1} << static_cast<unsigned>(type); }

[[nodiscard]] constexpr bool matches(PoiMask mask, PoiType type) noexcept { return (mask & maskOf(type)) != 0; }

namespace poiTypes {

/// parity: `PoiTypeTags.ACQUIRABLE_JOB_SITE`, the thirteen workstations.
inline constexpr PoiMask kAcquirableJobSites = ((maskOf(PoiType::kWeaponsmith) << 1) - 1) & ~maskOf(PoiType::kNone);

/// parity: `PoiTypeTags.VILLAGE`: every workstation, the beds and the bell.
inline constexpr PoiMask kVillage = kAcquirableJobSites | maskOf(PoiType::kHome) | maskOf(PoiType::kMeeting);

/// parity: `PoiTypeTags.BEE_HOME`.
inline constexpr PoiMask kBeeHome = maskOf(PoiType::kBeehive) | maskOf(PoiType::kBeeNest);

inline constexpr PoiMask kAll = (maskOf(PoiType::kCount) - 1) & ~maskOf(PoiType::kNone);

/// parity: `PoiTypes.forState`, which only the head of a bed answers for.
[[nodiscard]] PoiType forState(std::int32_t stateId);

[[nodiscard]] std::int32_t maxTickets(PoiType type);

[[nodiscard]] std::int32_t validRange(PoiType type);

/// The registry path, `home` for `minecraft:home`.
[[nodiscard]] std::string_view name(PoiType type);

/// Accepts the path alone or the full id.
[[nodiscard]] std::optional<PoiType> byName(std::string_view name);

/// parity: `VillagerProfession.heldJobSite`: the one workstation a profession works at, or none for
/// the unemployed and the nitwit.
[[nodiscard]] PoiType jobSiteOf(std::string_view profession);

/// parity: the first profession whose job site is this workstation, as `AssignProfessionFromJobSite`
/// finds it. Empty for anything that is not a workstation.
[[nodiscard]] std::string_view professionAt(PoiType type);

} // namespace poiTypes

} // namespace basalt::server::runtime::world::poi
