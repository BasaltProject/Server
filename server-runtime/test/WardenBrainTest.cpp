#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>

#include <doctest.h>

#include <algorithm>

using basalt::server::runtime::entity::ServerEntity;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
using basalt::wire::protocol::play::Pose;
namespace brain = basalt::server::runtime::entity::wardenBrain;
namespace wardens = basalt::server::runtime::entity::wardens;

namespace {

[[nodiscard]] ServerEntity warden() {
    return ServerEntity{1, Uuid{1, 1}, Identifier::vanilla("warden"), 0, 0.0, 64.0, 0.0};
}

[[nodiscard]] bool has(const std::vector<brain::Action>& actions, brain::Action::Kind kind) {
    return std::ranges::any_of(actions, [kind](const brain::Action& action) { return action.kind == kind; });
}

} // namespace

TEST_CASE("an emerging warden is untouchable and deaf until it stands") {
    ServerEntity mob = warden();
    const std::vector<brain::Action> started = brain::startEmerging(mob);
    CHECK(brain::poseOf(mob) == Pose::Emerging);
    CHECK(brain::isDiggingOrEmerging(mob));
    CHECK(wardens::anger(mob, 7, 50) == 0);
    for (std::int32_t tick = 0; tick < brain::kEmergeTicks - 1; ++tick) {
        static_cast<void>(brain::tick(mob, {.onGround = true}));
    }
    CHECK(brain::poseOf(mob) == Pose::Emerging);
    static_cast<void>(brain::tick(mob, {.onGround = true}));
    CHECK(brain::poseOf(mob) == Pose::Standing);
    CHECK(wardens::anger(mob, 7, 50) == 50);
}

TEST_CASE("a warden left alone for a minute burrows away, unless it is persistent") {
    ServerEntity mob = warden();
    std::int32_t discardAt = 0;
    for (std::int32_t tick = 1; tick <= 1400 && discardAt == 0; ++tick) {
        if (has(brain::tick(mob, {.onGround = true}), brain::Action::Kind::Discard)) {
            discardAt = tick;
        }
    }
    CHECK(discardAt == brain::kDigCooldown + 1 + brain::kDigTicks - 1);

    ServerEntity kept = warden();
    for (std::int32_t tick = 0; tick < 3000; ++tick) {
        CHECK_FALSE(has(brain::tick(kept, {.onGround = true, .persistent = true}), brain::Action::Kind::Discard));
    }
}

TEST_CASE("a roar comes before the fight, and the first boom waits ten seconds") {
    ServerEntity mob = warden();
    static_cast<void>(brain::startRoar(mob, 9));
    CHECK(brain::poseOf(mob) == Pose::Roaring);
    bool fought = false;
    for (std::int32_t tick = 0; tick < brain::kRoarTicks && !fought; ++tick) {
        fought = has(brain::tick(mob, {.onGround = true}), brain::Action::Kind::StartFight);
    }
    CHECK(fought);
    CHECK(brain::stateOf(mob).sonicCooldown == brain::kFirstSonicCooldown);
    // A target within reach gets its boom once the cooldown runs out, thirty-four ticks later.
    const brain::View view{.target = std::array<double, 3>{5.0, 64.0, 0.0}, .onGround = true};
    std::int32_t charge = 0;
    std::int32_t blast = 0;
    for (std::int32_t tick = 1; tick <= 400 && blast == 0; ++tick) {
        const std::vector<brain::Action> actions = brain::tick(mob, view);
        if (charge == 0 && has(actions, brain::Action::Kind::EntityEvent)) {
            charge = tick;
        }
        if (has(actions, brain::Action::Kind::SonicBlast)) {
            blast = tick;
        }
    }
    CHECK(charge == brain::kFirstSonicCooldown);
    CHECK(blast - charge == brain::kSonicBlastAt);
}

TEST_CASE("a swing pushes the next boom back") {
    ServerEntity mob = warden();
    static_cast<void>(brain::meleeHit(mob));
    CHECK(brain::stateOf(mob).sonicCooldown == brain::kSonicCooldown);
}
