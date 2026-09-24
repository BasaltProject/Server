#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/VillagerSchedule.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>

#include <doctest.h>

#include <cstdint>
#include <memory>

using basalt::server::runtime::entity::ServerEntity;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace brain = basalt::server::runtime::entity::villager::villagerBrain;
namespace schedule = basalt::server::runtime::entity::ai::villagerSchedule;

namespace {

[[nodiscard]] std::unique_ptr<ServerEntity> villagerAt(std::int32_t entityId, double x, double y, double z) {
    const basalt::server::registry::EntityTypeEntry* type =
        basalt::server::registry::entityTypes::of(Identifier::vanilla("villager"));
    REQUIRE(type != nullptr);
    return std::make_unique<ServerEntity>(entityId, Uuid{static_cast<std::uint64_t>(entityId), 1}, type->id,
                                          type->protocolId, x, y, z);
}

} // namespace

TEST_CASE("a villager's day follows the schedule timeline, rest before its first keyframe") {
    CHECK(schedule::activityAt(0) == schedule::Activity::Rest);
    CHECK(schedule::activityAt(10) == schedule::Activity::Idle);
    CHECK(schedule::activityAt(5000) == schedule::Activity::Work);
    CHECK(schedule::activityAt(10000) == schedule::Activity::Meet);
    CHECK(schedule::activityAt(11500) == schedule::Activity::Idle);
    CHECK(schedule::activityAt(15000) == schedule::Activity::Rest);
    CHECK(schedule::activityAt(24000 + 5000) == schedule::Activity::Work);
    CHECK(schedule::activityAt(-1000) == schedule::Activity::Rest);
    CHECK(schedule::activityAt(4000, true) == schedule::Activity::Play);
    CHECK(schedule::activityAt(7000, true) == schedule::Activity::Idle);
    CHECK(schedule::activityAt(11000, true) == schedule::Activity::Play);
}

TEST_CASE("only a villager that slept within the day and has seen no golem wants one") {
    const std::unique_ptr<ServerEntity> villager = villagerAt(1, 0.0, 64.0, 0.0);
    CHECK_FALSE(brain::wantsGolem(*villager, 100));

    brain::stateOf(*villager).memories.lastSlept.set(0);
    CHECK(brain::wantsGolem(*villager, 100));
    CHECK_FALSE(brain::wantsGolem(*villager, 24000));

    brain::golemDetected(*villager);
    CHECK_FALSE(brain::wantsGolem(*villager, 100));
}
