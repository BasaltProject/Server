#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WitherBoss.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <numbers>

namespace basalt::server::runtime::entity::witherBoss {
namespace {

using namespace std::string_view_literals;

/// parity: `#minecraft:undead`, with its two nested tags spelt out.
constexpr std::array kUndead{
    "skeleton"sv,     "stray"sv,      "wither_skeleton"sv, "skeleton_horse"sv,  "bogged"sv,           "parched"sv,
    "zombie_horse"sv, "camel_husk"sv, "zombie"sv,          "zombie_villager"sv, "zombified_piglin"sv, "zoglin"sv,
    "drowned"sv,      "husk"sv,       "zombie_nautilus"sv, "wither"sv,          "phantom"sv};

constexpr std::int32_t kDestroyBlocksDelay = 20;
constexpr std::int32_t kIdleHeadHurry = 3;
constexpr float kSideHeadReach = 1.3F;
constexpr float kMiddleHeadHeight = 3.0F;
constexpr float kSideHeadHeight = 2.2F;

} // namespace

bool isWither(const wire::common::Identifier& type) { return type.path() == "wither"; }

bool isFriend(std::string_view typePath) { return std::ranges::find(kUndead, typePath) != kUndead.end(); }

bool isPowered(const ServerEntity& wither) { return wither.health <= kMaxHealth / 2.0; }

std::array<double, 3> headPosition(const ServerEntity& wither, std::int32_t head) {
    const double y = wither.y + static_cast<double>(head <= 0 ? kMiddleHeadHeight : kSideHeadHeight);
    if (head <= 0) {
        return {wither.x, y, wither.z};
    }
    // parity: the body's yaw turned half a circle for the second side head.
    const float angle =
        (wither.yaw + static_cast<float>(180 * (head - 1))) * static_cast<float>(std::numbers::pi / 180.0);
    return {wither.x + (static_cast<double>(wire::common::mth::cos(angle)) * static_cast<double>(kSideHeadReach)), y,
            wither.z + (static_cast<double>(wire::common::mth::sin(angle)) * static_cast<double>(kSideHeadReach))};
}

void makeInvulnerable(ServerEntity& wither, WitherState& state) {
    state.invulnerableTicks = kInvulnerableTicks;
    wither.health = kMaxHealth / 3.0;
}

bool refuses(const ServerEntity& wither, WitherState& state, const Hit& hit) {
    if (hit.witherImmuneType || hit.fromWitherFriend) {
        return true;
    }
    if (state.invulnerableTicks > 0 && !hit.bypassesInvulnerability) {
        return true;
    }
    if (isPowered(wither) && hit.arrowOrWindCharge) {
        return true;
    }
    if (state.destroyBlocksTick <= 0) {
        state.destroyBlocksTick = kDestroyBlocksDelay;
    }
    for (std::int32_t& idle : state.idleHeadUpdates) {
        idle += kIdleHeadHurry;
    }
    return false;
}

} // namespace basalt::server::runtime::entity::witherBoss
