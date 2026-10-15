#pragma once

#include <cstdint>

namespace basalt::server::runtime::entity::ai {

/// parity: `Schedule.VILLAGER_DEFAULT`.
namespace villagerSchedule {

enum class Activity : std::uint8_t { Idle, Work, Meet, Rest, Play };

/// parity: the `villager_schedule` timeline's two tracks, the baby's and the adult's.
[[nodiscard]] Activity activityAt(std::int64_t dayTime, bool baby = false);

} // namespace villagerSchedule

} // namespace basalt::server::runtime::entity::ai
