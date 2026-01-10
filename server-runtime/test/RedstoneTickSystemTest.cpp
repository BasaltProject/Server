#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/EnvironmentTickSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/RedstoneTickSystem.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PressurePlates.hpp>
#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
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
using basalt::server::runtime::RedstoneTickSystem;
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

namespace {

struct RedstoneFixture {
    EnvironmentFixture env;
    basalt::server::runtime::world::RealRecipes recipes{
        {.readText = [](std::string_view) { return std::optional<std::string>{}; },
         .entriesUnder = [](std::string_view) { return std::vector<std::string>{}; }}};
    RedstoneTickSystem redstone{env.spawn.base.registry, recipes, env.broadcaster,
                                env.simulation,          {},      {.nextSeed = [] { return std::int64_t{17}; }}};
    basalt::server::runtime::sched::RegionScheduler scheduler;

    explicit RedstoneFixture(const std::string& root) : env(root) {}

    [[nodiscard]] basalt::server::runtime::sched::Region& region() { return scheduler.regionFor("world"); }
};

} // namespace

TEST_CASE("standing on a plate presses it and stepping off books its way back up") {
    RedstoneFixture fixture{scratch("redstone-plate")};
    LoadedWorld& world = *fixture.env.spawn.base.world;
    const std::int32_t plate = blocks::named("oak_pressure_plate")->stateId;
    REQUIRE(world.state().setBlockState(8, 64, 8, plate));

    PlayerSession& player = *fixture.env.spawn.base.player;
    player.x = 8.5;
    player.y = 64.0;
    player.z = 8.5;

    fixture.redstone.tickPressurePlates(world, fixture.region());
    const std::optional<std::int32_t> pressed = world.state().blockState(8, 64, 8);
    REQUIRE(pressed.has_value());
    CHECK(*pressed != plate);
}

TEST_CASE("a plate nobody is on is left alone") {
    RedstoneFixture fixture{scratch("redstone-plate-empty")};
    LoadedWorld& world = *fixture.env.spawn.base.world;
    const std::int32_t plate = blocks::named("oak_pressure_plate")->stateId;
    REQUIRE(world.state().setBlockState(8, 64, 8, plate));

    PlayerSession& player = *fixture.env.spawn.base.player;
    player.x = 40.5;
    player.y = 64.0;
    player.z = 40.5;

    fixture.redstone.tickPressurePlates(world, fixture.region());
    CHECK(world.state().blockState(8, 64, 8) == plate);
}

TEST_CASE("a chunk being simulated ages while somebody is near it") {
    RedstoneFixture fixture{scratch("redstone-inhabited")};
    LoadedWorld& world = *fixture.env.spawn.base.world;
    PlayerSession& player = *fixture.env.spawn.base.player;
    player.x = 8.0;
    player.y = 64.0;
    player.z = 8.0;
    const basalt::server::world::Chunk* chunk = world.state().chunkIfLoaded(0, 0);
    REQUIRE(chunk != nullptr);
    const std::int64_t before = chunk->inhabitedTime();

    fixture.redstone.tickInhabitedTime(world, fixture.region());
    CHECK(chunk->inhabitedTime() > before);
}

TEST_CASE("a world with nothing due does nothing at all") {
    RedstoneFixture fixture{scratch("redstone-quiet")};
    LoadedWorld& world = *fixture.env.spawn.base.world;
    fixture.env.spawn.base.packets.clear();

    fixture.redstone.tickFluids(world, fixture.region());
    fixture.redstone.tickScheduledBlocks(world, fixture.region());
    fixture.redstone.tickFallingBlocks(world, fixture.region());
    CHECK(fixture.env.spawn.base.packets.empty());
}
