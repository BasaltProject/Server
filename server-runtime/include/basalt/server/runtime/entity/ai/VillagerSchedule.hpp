#pragma once

#include <cstdint>

namespace basalt::server::runtime::entity::ai {

/// What a villager is meant to be doing at a given time of day.
///
/// parity: `Schedule.VILLAGER_DEFAULT`, whose four windows are what makes villagers work in the
/// morning, gather at the bell at midday and go to bed at dusk.
namespace villagerSchedule {

enum class Activity : std::uint8_t { Idle, Work, Meet, Rest };

[[nodiscard]] Activity activityAt(std::int64_t dayTime);

} // namespace villagerSchedule

} // namespace basalt::server::runtime::entity::ai
