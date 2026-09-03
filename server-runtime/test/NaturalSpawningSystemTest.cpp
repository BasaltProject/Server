#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/SpawnEquipment.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::NaturalSpawningSystem;
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

TEST_CASE("a mob asked for appears where it was asked for") {
    SpawnFixture fixture{scratch("spawning-basic")};
    NaturalSpawningSystem system = fixture.system();

    ServerEntity* cow = system.spawnMob(*fixture.base.world, Identifier::vanilla("cow"), 8.5, 64.0, 8.5, 90.0F);
    REQUIRE(cow != nullptr);
    CHECK(cow->type().path() == "cow");
    CHECK(cow->x == doctest::Approx(8.5));
    CHECK(cow->yaw == doctest::Approx(90.0F));
    CHECK(fixture.base.world->state().entity(cow->entityId()) == cow);

    // A type this build has no entry for is not something to invent an id for.
    CHECK(system.spawnMob(*fixture.base.world, Identifier::vanilla("not_a_mob"), 8.5, 64.0, 8.5) == nullptr);
}

TEST_CASE("a plugin that refuses a spawn leaves the world without it") {
    SpawnFixture fixture{scratch("spawning-refused")};
    std::vector<std::string> reasons;
    NaturalSpawningSystem system =
        fixture.system({.spawnAllowed = [&reasons](const LoadedWorld&, const Identifier&, double, double, double,
                                                   std::string_view reason) {
            reasons.emplace_back(reason);
            return false;
        }});

    CHECK(system.spawnMob(*fixture.base.world, Identifier::vanilla("cow"), 8.5, 64.0, 8.5) == nullptr);
    REQUIRE(reasons.size() == 1);
    // The reason travels with it, since that is the one thing a plugin always filters on.
    CHECK(reasons.front() == "natural");
    CHECK(fixture.base.world->state().entities().empty());
}

TEST_CASE("the thunder horse is a trap rather than an ordinary skeleton horse") {
    SpawnFixture fixture{scratch("spawning-trap")};
    NaturalSpawningSystem system = fixture.system();

    ServerEntity* horse = system.spawnSkeletonTrap(*fixture.base.world, 8, 64, 8);
    REQUIRE(horse != nullptr);
    CHECK(horse->skeletonTrap);
    CHECK(horse->skeletonTrapTime == 0);
    CHECK(horse->age == 0);

    // Only a trap horse activates, and only once.
    basalt::server::random::RandomSource random = basalt::server::random::RandomSource::legacy(11);
    CHECK(system.activateSkeletonTrap(*fixture.base.world, *horse, random));

    ServerEntity* plain = system.spawnMob(*fixture.base.world, Identifier::vanilla("skeleton_horse"), 8.5, 64.0, 8.5);
    REQUIRE(plain != nullptr);
    CHECK_FALSE(system.activateSkeletonTrap(*fixture.base.world, *plain, random));
}

TEST_CASE("spawning is off while the game rule says so") {
    SpawnFixture fixture{scratch("spawning-rule")};
    NaturalSpawningSystem system = fixture.system();
    basalt::server::runtime::sched::RegionScheduler scheduler;
    basalt::server::runtime::sched::Region& region = scheduler.regionFor("world");

    fixture.base.world->gameRules().set(basalt::server::runtime::world::gameRules::kSpawnMobs, "false");
    system.tickSpawning(*fixture.base.world, region);
    CHECK(fixture.base.world->state().entities().empty());
}
