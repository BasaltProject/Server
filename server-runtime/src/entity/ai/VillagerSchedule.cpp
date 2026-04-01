#include <basalt/server/runtime/entity/ai/VillagerSchedule.hpp>

#include <cstdint>

namespace basalt::server::runtime::entity::ai::villagerSchedule {
namespace {

constexpr std::int64_t kDay = 24000;
/// parity: the first keyframe of both tracks; before it the day still holds the last one, rest.
constexpr std::int32_t kFirstKeyframe = 10;

} // namespace

Activity activityAt(std::int64_t dayTime, bool baby) {
    const auto time = static_cast<std::int32_t>(((dayTime % kDay) + kDay) % kDay);
    if (time < kFirstKeyframe || time >= 12000) {
        return Activity::Rest;
    }
    if (baby) {
        if (time < 3000) {
            return Activity::Idle;
        }
        if (time < 6000) {
            return Activity::Play;
        }
        return time < 10000 ? Activity::Idle : Activity::Play;
    }
    if (time < 2000) {
        return Activity::Idle;
    }
    if (time < 9000) {
        return Activity::Work;
    }
    return time < 11000 ? Activity::Meet : Activity::Idle;
}

} // namespace basalt::server::runtime::entity::ai::villagerSchedule
