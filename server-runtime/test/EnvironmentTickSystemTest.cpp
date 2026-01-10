#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/EnvironmentTickSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/SpawnEquipment.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::BlockChangeBroadcaster;
using basalt::server::runtime::EnvironmentTickSystem;
using basalt::server::runtime::NaturalSpawningSystem;
using basalt::server::runtime::command::CommandEnvironment;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::RealLootTables;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace blocks = basalt::server::registry::blocks;
namespace config = basalt::server::runtime::config;
namespace play = basalt::wire::protocol::play;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

struct Fixture {
    PlayerRegistry registry;
    std::unique_ptr<LoadedWorld> world;
    std::unique_ptr<PlayerSession> player;
    std::vector<std::int32_t> packets;
    float hurt = 0.0F;
    std::vector<std::pair<std::string, std::int32_t>> effects;
    std::map<std::string, std::string> lootFiles;

    explicit Fixture(const std::string& root)
        : world(std::make_unique<LoadedWorld>(
              config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
              config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"})) {
        PlayerConnection connection;
        connection.sendPlayPacket = [this](std::int32_t id,
                                           const std::function<void(basalt::wire::io::ByteBuffer&)>& write) {
            basalt::wire::io::ByteBuffer buffer;
            write(buffer);
            packets.push_back(id);
        };
        connection.hurt = [this](float amount, std::string_view, const Identifier&,
                                 const std::optional<basalt::server::runtime::entity::DamageSource>&) {
            hurt += amount;
        };
        connection.addEffect = [this](const Identifier& effect, std::int32_t ticks, std::int32_t) {
            effects.emplace_back(effect.path(), ticks);
        };
        player = std::make_unique<PlayerSession>(Uuid{1, 1}, "Steve", 1, std::move(connection));
        player->worldName = "world";
        registry.add(player.get());
        world->state().getOrGenerateChunk(0, 0);
    }

    [[nodiscard]] std::vector<PlayerSession*> watchers() { return {player.get()}; }

    [[nodiscard]] RealLootTables::Jar jar() {
        return {.readText = [this](std::string_view name) -> std::optional<std::string> {
                    const auto found = lootFiles.find(std::string{name});
                    return found == lootFiles.end() ? std::nullopt : std::optional{found->second};
                },
                .entriesUnder = [](std::string_view) { return std::vector<std::string>{}; }};
    }

    [[nodiscard]] std::int32_t sentCount(std::int32_t id) const {
        return static_cast<std::int32_t>(std::ranges::count(packets, id));
    }
};

} // namespace

namespace {

struct SpawnFixture {
    Fixture base;
    basalt::server::runtime::world::RealBiomeSpawns biomeSpawns{
        {.readText = [](std::string_view) { return std::optional<std::string>{}; }}};
    basalt::server::runtime::world::SpawnEquipmentFinalizer equipment{
        {.entriesUnder = [](std::string_view) { return std::vector<std::string>{}; },
         .readText = [](std::string_view) { return std::optional<std::string>{}; }}};
    basalt::server::runtime::ChunkSimulationView simulation{base.registry};

    explicit SpawnFixture(const std::string& root) : base(root) {}

    [[nodiscard]] NaturalSpawningSystem system(NaturalSpawningSystem::Hooks hooks = {}) {
        return NaturalSpawningSystem{base.registry, biomeSpawns,      simulation,
                                     equipment,     std::move(hooks), {.nextSeed = [] { return std::int64_t{7}; }}};
    }
};

} // namespace

namespace {

struct EnvironmentFixture {
    SpawnFixture spawn;
    RealLootTables tables{spawn.base.jar()};
    BlockChangeBroadcaster broadcaster{spawn.base.registry, tables, {}, {.nextSeed = [] { return std::int64_t{5}; }}};
    NaturalSpawningSystem spawning = spawn.system();
    basalt::server::runtime::ChunkSimulationView simulation{spawn.base.registry};
    CommandEnvironment environment{spawn.base.registry, {}};
    config::WorldsConfig worldsConfig{{config::WorldDeclaration{.name = "world", .isDefault = true}}, {}, "worlds"};
    EnvironmentTickSystem tick{spawn.base.registry,
                               worldsConfig,
                               [this] { return std::vector<LoadedWorld*>{spawn.base.world.get()}; },
                               simulation,
                               broadcaster,
                               environment,
                               spawning,
                               {},
                               {.nextSeed = [] { return std::int64_t{9}; }}};

    explicit EnvironmentFixture(const std::string& root) : spawn(root) {}
};

} // namespace

TEST_CASE("enough players asleep for long enough skips the night") {
    EnvironmentFixture fixture{scratch("environment-sleep")};
    LoadedWorld& world = *fixture.spawn.base.world;
    world.clock().setTotalTicks(13000);
    world.weather().raining = true;

    PlayerSession& player = *fixture.spawn.base.player;
    player.sleepingBed = std::array<std::int32_t, 3>{8, 64, 8};
    player.sleepTicks = basalt::server::runtime::world::beds::kSleepTicksBeforeMorning;

    fixture.tick.tickSleep(world);
    CHECK(world.clock().totalTicks() % 24000 == 0);
    // parity: `ServerLevel.tickTime`'s sleep path, which stops the rain along with the night.
    CHECK_FALSE(world.weather().raining);
}

TEST_CASE("one player who has only just lain down does not skip anything") {
    EnvironmentFixture fixture{scratch("environment-sleep-early")};
    LoadedWorld& world = *fixture.spawn.base.world;
    world.clock().setTotalTicks(13000);

    PlayerSession& player = *fixture.spawn.base.player;
    player.sleepingBed = std::array<std::int32_t, 3>{8, 64, 8};
    player.sleepTicks = 1;

    fixture.tick.tickSleep(world);
    CHECK(world.clock().totalTicks() == 13000);
}

TEST_CASE("standing underwater empties the air supply and then hurts") {
    EnvironmentFixture fixture{scratch("environment-drown")};
    LoadedWorld& world = *fixture.spawn.base.world;
    namespace fluids = basalt::server::runtime::world::fluids;
    const std::int32_t water = blocks::named("water")->stateId;
    for (std::int32_t y = 63; y <= 66; y++) {
        REQUIRE(world.state().setBlockState(8, y, 8, water));
    }
    PlayerSession& player = *fixture.spawn.base.player;
    player.x = 8.5;
    player.y = 64.0;
    player.z = 8.5;
    // parity: `shouldTakeDrowningDamage`, where the air runs twenty ticks past empty first.
    player.airSupply = basalt::server::runtime::world::fluids::kDrowningAir + 1;

    fixture.tick.tickSubmersion(world);
    CHECK(player.airSupply == 0);
    CHECK(fixture.spawn.base.hurt == doctest::Approx(fluids::kDrownDamage));
}

TEST_CASE("dry land refills the air supply without hurting anybody") {
    EnvironmentFixture fixture{scratch("environment-dry")};
    PlayerSession& player = *fixture.spawn.base.player;
    player.x = 8.5;
    player.y = 64.0;
    player.z = 8.5;
    player.airSupply = 10;

    fixture.tick.tickSubmersion(*fixture.spawn.base.world);
    CHECK(player.airSupply > 10);
    CHECK(fixture.spawn.base.hurt == doctest::Approx(0.0));
}

TEST_CASE("a portal cooldown counts down instead of taking anybody anywhere") {
    EnvironmentFixture fixture{scratch("environment-portal-cooldown")};
    PlayerSession& player = *fixture.spawn.base.player;
    player.portalCooldown = 3;
    player.portalTicks = 7;

    fixture.tick.tickPortals(*fixture.spawn.base.world);
    CHECK(player.portalCooldown == 2);
    CHECK(player.portalTicks == 0);
}
