#pragma once

#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

enum class BedProblem : std::uint8_t { NotPossibleNow, TooFarAway, Obstructed, Occupied, NotHere };

[[nodiscard]] std::string_view translationKeyOf(BedProblem problem);

namespace beds {

inline constexpr std::int64_t kNightStart = 12542;
inline constexpr std::int64_t kNightEnd = 23459;

inline constexpr double kRange = 3.0;
inline constexpr double kVerticalRange = 2.0;

inline constexpr std::int32_t kSleepTicksBeforeMorning = 100;

[[nodiscard]] bool isBed(std::int32_t stateId);

[[nodiscard]] bool isOccupied(std::int32_t stateId);

[[nodiscard]] std::string facingOf(std::int32_t stateId);

[[nodiscard]] bool isHead(std::int32_t stateId);

[[nodiscard]] std::int32_t withOccupied(std::int32_t stateId, bool occupied);

[[nodiscard]] std::array<std::int32_t, 3> headOffset(std::int32_t stateId);

enum class When : std::uint8_t { Always, WhenDark, Never };

struct Rule {
    When canSleep = When::WhenDark;
    When canSetSpawn = When::Always;
    bool destroyOnUse = false;
    bool destroyOnLeave = false;
    std::string_view errorKey;
};

[[nodiscard]] bool holds(When when, bool darkOutside);

/// The rule a bed follows in a dimension. parity: the vanilla `dimension_type` attributes.
[[nodiscard]] Rule ruleFor(config::WorldDimension dimension, bool strawBed);

[[nodiscard]] bool isStrawBed(std::int32_t stateId);

[[nodiscard]] bool reachable(double x, double y, double z, std::int32_t bedX, std::int32_t bedY, std::int32_t bedZ);

[[nodiscard]] bool isNight(std::int64_t timeOfDay);

[[nodiscard]] std::int64_t nextMorning(std::int64_t timeOfDay);

[[nodiscard]] std::vector<BlockChange> destroy(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] bool isBedItem(const wire::common::Identifier& item);

} // namespace beds

} // namespace basalt::server::runtime::world
