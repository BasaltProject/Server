#pragma once

#include <cstdint>

namespace basalt::server::runtime::entity::ai {

/// What a villager is meant to be doing at a given time of day.
///
/// parity: `Schedule.VILLAGER_DEFAULT`, whose four windows are what makes villagers work in the
/// morning, gather at the bell at midday and go to bed at dusk.
namespace villagerSchedule {

enum class Activity : std::uint8_t { Idle, Work, Meet, Rest, Play };

/// parity: the `villager_schedule` timeline's two tracks, the baby's and the adult's.
[[nodiscard]] Activity activityAt(std::int64_t dayTime, bool baby = false);

} // namespace villagerSchedule

} // namespace basalt::server::runtime::entity::ai
