#include <basalt/server/runtime/entity/ai/VillagerSchedule.hpp>

#include <cstdint>

namespace basalt::server::runtime::entity::ai::villagerSchedule {
namespace {

constexpr std::int64_t kDay = 24000;
constexpr std::int32_t kWorkStart = 2000;
constexpr std::int32_t kMeetStart = 9000;
constexpr std::int32_t kEveningStart = 11000;
constexpr std::int32_t kRestStart = 12000;

} // namespace

Activity activityAt(std::int64_t dayTime) {
    const auto time = static_cast<std::int32_t>(((dayTime % kDay) + kDay) % kDay);
    if (time < kWorkStart) {
        return Activity::Idle;
    }
    if (time < kMeetStart) {
        return Activity::Work;
    }
    if (time < kEveningStart) {
        return Activity::Meet;
    }
    return time < kRestStart ? Activity::Idle : Activity::Rest;
}

} // namespace basalt::server::runtime::entity::ai::villagerSchedule
