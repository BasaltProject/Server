#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
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
using basalt::server::runtime::NaturalSpawningSystem;
using basalt::server::runtime::RaidSystem;
using basalt::server::runtime::entity::Raid;
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

struct RaidFixture {
    SpawnFixture spawn;
    RealLootTables tables{spawn.base.jar()};
    BlockChangeBroadcaster broadcaster{spawn.base.registry, tables, {}, {.nextSeed = [] { return std::int64_t{5}; }}};
    NaturalSpawningSystem spawning = spawn.system();
    std::vector<bool> finished;
    RaidSystem raids{spawn.base.registry,
                     broadcaster,
                     spawning,
                     {.raidFinished = [this](const LoadedWorld&, const Raid&, bool won) { finished.push_back(won); }},
                     {.nextSeed = [] { return std::int64_t{13}; }}};

    explicit RaidFixture(const std::string& root) : spawn(root) {}
};

} // namespace

TEST_CASE("a bad omen carried into a village turns into a raid omen") {
    RaidFixture fixture{scratch("raid-omen")};
    LoadedWorld& world = *fixture.spawn.base.world;
    // Three beds is what makes a place a village here, since there is no point of interest map.
    const std::int32_t bed = blocks::named("red_bed")->stateId;
    REQUIRE(world.state().setBlockState(8, 64, 8, bed));
    REQUIRE(world.state().setBlockState(9, 64, 8, bed));
    REQUIRE(world.state().setBlockState(10, 64, 8, bed));
    fixture.spawn.base.player->x = 8.0;
    fixture.spawn.base.player->y = 64.0;
    fixture.spawn.base.player->z = 8.0;

    std::vector<std::pair<std::string, std::int32_t>> started;
    basalt::server::runtime::world::ActiveEffect omen{
        .effect = basalt::server::runtime::world::mobEffects::badOmen(), .amplifier = 0, .remainingTicks = 100};
    fixture.raids.tickOmens(world, *fixture.spawn.base.player, omen);

    REQUIRE(fixture.spawn.base.effects.size() == 1);
    CHECK(fixture.spawn.base.effects.front().first == "raid_omen");
    // Where they were standing is where the raid will start.
    REQUIRE(fixture.spawn.base.player->raidOmenPosition.has_value());
    CHECK((*fixture.spawn.base.player->raidOmenPosition)[0] == 8);
}

TEST_CASE("a bad omen away from any village does nothing at all") {
    RaidFixture fixture{scratch("raid-no-village")};
    basalt::server::runtime::world::ActiveEffect omen{
        .effect = basalt::server::runtime::world::mobEffects::badOmen(), .amplifier = 0, .remainingTicks = 100};
    fixture.raids.tickOmens(*fixture.spawn.base.world, *fixture.spawn.base.player, omen);
    CHECK(fixture.spawn.base.effects.empty());
    CHECK_FALSE(fixture.spawn.base.player->raidOmenPosition.has_value());
}

TEST_CASE("a raid omen running out starts the raid where it was picked up") {
    RaidFixture fixture{scratch("raid-start")};
    LoadedWorld& world = *fixture.spawn.base.world;
    fixture.spawn.base.player->raidOmenPosition = std::array<std::int32_t, 3>{8, 64, 8};
    basalt::server::runtime::world::ActiveEffect ending{
        .effect = basalt::server::runtime::world::mobEffects::raidOmen(), .amplifier = 1, .remainingTicks = 1};

    fixture.raids.tickOmens(world, *fixture.spawn.base.player, ending);
    REQUIRE(fixture.raids.raidsIn(world).all().size() == 1);
    const Raid& raid = *fixture.raids.raidsIn(world).all().front();
    CHECK(raid.centerX() == 8);
    // parity: the omen's own amplifier is what the raid's level starts at.
    CHECK(raid.omenLevel == 2);
    CHECK_FALSE(fixture.spawn.base.player->raidOmenPosition.has_value());
}

TEST_CASE("a raid with no waves left is won and its heroes rewarded") {
    RaidFixture fixture{scratch("raid-victory")};
    LoadedWorld& world = *fixture.spawn.base.world;
    Raid* raid = fixture.raids.raidsIn(world).createOrExtend(8, 64, 8, 0, "easy");
    REQUIRE(raid != nullptr);
    raid->groupsSpawned = raid->totalGroups();
    fixture.spawn.base.player->x = 8.0;
    fixture.spawn.base.player->y = 64.0;
    fixture.spawn.base.player->z = 8.0;

    fixture.raids.tickRaids(world);
    CHECK(raid->status == basalt::server::runtime::entity::RaidStatus::Victory);
    REQUIRE(fixture.spawn.base.effects.size() == 1);
    CHECK(fixture.spawn.base.effects.front().first == "hero_of_the_village");

    // It stays around for its celebration and is only then forgotten.
    CHECK(fixture.raids.raidsIn(world).all().size() == 1);
    CHECK(fixture.finished.empty());
}

TEST_CASE("a raid that runs out of time stops without a victory") {
    RaidFixture fixture{scratch("raid-timeout")};
    LoadedWorld& world = *fixture.spawn.base.world;
    Raid* raid = fixture.raids.raidsIn(world).createOrExtend(8, 64, 8, 0, "easy");
    REQUIRE(raid != nullptr);
    raid->ticksActive = Raid::kTimeoutTicks - 1;

    fixture.raids.tickRaids(world);
    // Nothing is celebrating, so it is gone the same tick, which is why nothing here reads `raid`
    // again: the manager owns it and has already dropped it.
    CHECK(fixture.raids.raidsIn(world).all().empty());
    REQUIRE(fixture.finished.size() == 1);
    CHECK_FALSE(fixture.finished.front());
}
