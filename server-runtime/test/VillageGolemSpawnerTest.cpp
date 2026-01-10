#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/VillagerSchedule.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/VillageGolemSpawner.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using basalt::server::random::RandomSource;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace config = basalt::server::runtime::config;
namespace golem = basalt::server::runtime::world::villageGolemSpawner;
namespace schedule = basalt::server::runtime::entity::ai::villagerSchedule;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

[[nodiscard]] std::unique_ptr<ServerEntity> villagerAt(std::int32_t entityId, double x, double y, double z) {
    const basalt::server::registry::EntityTypeEntry* type =
        basalt::server::registry::entityTypes::of(Identifier::vanilla("villager"));
    REQUIRE(type != nullptr);
    return std::make_unique<ServerEntity>(entityId, Uuid{static_cast<std::uint64_t>(entityId), 1}, type->id,
                                          type->protocolId, x, y, z);
}

} // namespace

TEST_CASE("a villager's day is four windows, and midday is when they meet") {
    CHECK(schedule::activityAt(0) == schedule::Activity::Idle);
    CHECK(schedule::activityAt(5000) == schedule::Activity::Work);
    CHECK(schedule::activityAt(10000) == schedule::Activity::Meet);
    CHECK(schedule::activityAt(11500) == schedule::Activity::Idle);
    CHECK(schedule::activityAt(15000) == schedule::Activity::Rest);
    // The next day is the same day, and a negative clock is still a time of day.
    CHECK(schedule::activityAt(24000 + 5000) == schedule::Activity::Work);
    CHECK(schedule::activityAt(-1000) == schedule::Activity::Rest);
}

TEST_CASE("only a villager that slept recently and has seen no golem wants one") {
    const std::unique_ptr<ServerEntity> villager = villagerAt(1, 0.0, 64.0, 0.0);
    // Never slept: it does not want a golem however long it waits.
    CHECK_FALSE(golem::wantsGolem(*villager, 100));

    villager->lastSleptGameTime = 0;
    CHECK(golem::wantsGolem(*villager, 100));
    // A day later the memory of sleeping is gone.
    CHECK_FALSE(golem::wantsGolem(*villager, 24000));

    // And one that has just seen a golem does not want another.
    villager->golemDetectedTicks = 10;
    CHECK_FALSE(golem::wantsGolem(*villager, 100));
}

TEST_CASE("a lone villager never gets a golem, however long it panics") {
    const std::string root = scratch("golem-alone");
    LoadedWorld world{config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
                      config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"}};
    world.state().getOrGenerateChunk(0, 0);
    const std::unique_ptr<ServerEntity> villager = villagerAt(1, 8.0, -60.0, 8.0);
    villager->lastSleptGameTime = 0;
    villager->panicTicks = 100;
    RandomSource random = RandomSource::legacy(5);

    std::int32_t spawned = 0;
    for (std::int32_t tick = 0; tick < 2000; tick++) {
        golem::tick(world, *villager, {villager.get()}, random, [&spawned](const std::array<std::int32_t, 3>&) {
            spawned++;
            return true;
        });
    }
    // parity: three panicking villagers have to agree, and one of them is never three.
    CHECK(spawned == 0);
}
